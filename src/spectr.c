/* Bandpass filter
 *
 * Copyright (C) 2013 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#if __cplusplus >= 201103L || defined __APPLE__
# include <complex>
# define csqrt(XX) std::sqrt(XX)
# define creal(XX) std::real(XX)
# define cimag(XX) std::imag(XX)
# define _I ((complex_t)(1i))
  typedef std::complex<double> complex_t;
#else
# include <complex.h>
# define _I I
  typedef _Complex double complex_t;
#endif

enum filterCoeff {a0 = 0, a1, a2, b0, b1, b2};
enum filterState {z1 = 0, z2};

#define NODENORMAL (1e-12)

struct Filter {
	double W[6];
	double z[2];
};

struct FilterBank {
	struct Filter f[6];
	uint32_t filter_stages;
	bool ac;
};

static inline double
proc_one(struct Filter * const f, const double in)
{
	const double w   = in - f->W[a1]*f->z[z1] - f->W[a2]*f->z[z2];
	const double out =      f->W[b0]*w        + f->W[b1]*f->z[z1] + f->W[b2]*f->z[z2];
	f->z[z2] = f->z[z1];
	f->z[z1] = w;
	return out;
}

static inline float
bandpass_process(struct FilterBank * const fb, float in)
{
	fb->ac = !fb->ac;
	double out = in + ((fb->ac) ? NODENORMAL : -NODENORMAL);
	for (uint32_t i = 0; i < fb->filter_stages; ++i) {
		out = proc_one(&fb->f[i], out);
	}
	return out;
}

static void
bandpass_setup(struct FilterBank *fb,
		double rate,
		double freq,
		double band
		) {

	fb->filter_stages = 6; // must be an even number

	for (uint32_t i = 0; i < fb->filter_stages; ++i) {
		fb->f[i].z[z1] = fb->f[i].z[z2] = 0;
	}

	const double fc = M_PI * freq / rate;
	const double fw = M_PI * band / rate;

	// TODO check||limit to nyquist

	/* num/den coefficients */
	const double c_a  = cos (2. * fc) / cos (fw);
	const double c_b  = 1. / tan (fw);
	const double c_a2 = c_a * c_a;
	const double c_b2 = c_b * c_b;
	const double ab_2 = 2. * c_a * c_b;
	const double w    = 2. * atan (sqrt (tan (fc + fw * .5) * tan(fc - fw * .5)));

	/* bilinear transform coefficients into z-domain */
	for (uint32_t i = 0; i < fb->filter_stages / 2; ++i) {
		const double omega =  M_PI_2 + (2 * i + 1) * M_PI / (2. * (double)fb->filter_stages);
		complex_t p = cos (omega) +  _I * sin (omega);

		const complex_t c = (1. + p) / (1. - p);
		const complex_t d = 2 * (c_b - 1) * c + 2 * (1 + c_b);
		complex_t v;

		v = (4 * (c_b2 * (c_a2 - 1) + 1)) * c;
		v += 8 * (c_b2 * (c_a2 - 1) - 1);
		v *= c;
		v += 4 * (c_b2 * (c_a2 - 1) + 1);
		v = csqrt (v);

		const complex_t u0 = ab_2 + creal(-v) + ab_2 * creal(c) + _I * (cimag(-v) + ab_2 * cimag(c));
		const complex_t u1 = ab_2 + creal( v) + ab_2 * creal(c) + _I * (cimag( v) + ab_2 * cimag(c));

#define ASSIGN_BP(FLT, PC, odd) \
	{ \
		const complex_t P = PC; \
		(FLT).W[a0] = 1.; \
		(FLT).W[a1] = -2 * creal(P); \
		(FLT).W[a2] = creal(P) * creal(P) + cimag(P) * cimag(P); \
		(FLT).W[b0] = 1.; \
		(FLT).W[b1] = (odd) ? -2. : 2.; \
		(FLT).W[b2] = 1.; \
	}
		ASSIGN_BP(fb->f[2*i],   u0/d, 0);
		ASSIGN_BP(fb->f[2*i+1], u1/d, 1);
#undef ASSIGN_BP
	}

	/* normalize */
	const double cos_w = cos (-w);
	const double sin_w = sin (-w);
	const double cos_w2 = cos (-2. * w);
	const double sin_w2 = sin (-2. * w);
	complex_t ch = 1;
	complex_t cb = 1;
	for (uint32_t i = 0; i < fb->filter_stages; ++i) {
		ch *= ((1 + fb->f[i].W[b1] * cos_w) + cos_w2)
			  + _I * ((fb->f[i].W[b1] * sin_w) + sin_w2);
		cb *= ((1 + fb->f[i].W[a1] * cos_w) + fb->f[i].W[a2] * cos_w2)
			  + _I * ((fb->f[i].W[a1] * sin_w) + fb->f[i].W[a2] * sin_w2);
	}

	const complex_t scale = cb / ch;
	fb->f[0].W[b0] *= creal(scale);
	fb->f[0].W[b1] *= creal(scale);
	fb->f[0].W[b2] *= creal(scale);

#ifdef DEBUG_SPECTR
	printf("SCALE (%g,  %g)\n", creal(scale), cimag(scale));
	for (uint32_t i = 0; i < fb->filter_stages; ++i) {
		struct Filter *flt = &fb->f[i];
		printf("%d: %g %g %g  %+g %+g %+g\n", i,
				flt->W[a0], flt->W[a1], flt->W[a2],
				flt->W[b0], flt->W[b1], flt->W[b2]);
	}
#endif
}
