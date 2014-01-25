/* meter.lv2
 *
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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

/******************************************************************************
 * bandpass filter
 */

//#ifdef DEBUG_SPECTR

/******************************************************************************
 * LV2 spec
 */

#define FILTER_COUNT (30)

typedef enum {
	MF_INPUT0   = 60,
	MF_OUTPUT0  = 61,
	MF_INPUT1   = 62,
	MF_OUTPUT1  = 63,
} MFPortIndex;

struct stcorr {
	float zl;
	float zr;
	float zlr;
	float zll;
	float zrr;
};


static inline void stc_proc_one (struct stcorr *s, const float pl, const float pr, const float _w1, const float _w2) {
	s->zl  += _w1 * (pl - s->zl) + 1e-20f;
	s->zr  += _w1 * (pr - s->zr) + 1e-20f;
	s->zlr += _w2 * (s->zl * s->zr - s->zlr);
	s->zll += _w2 * (s->zl * s->zl - s->zll);
	s->zrr += _w2 * (s->zr * s->zr - s->zrr);
}

static inline void stc_proc_end (struct stcorr *s) {

	if (!finite(s->zl))  s->zl = 0;
	if (!finite(s->zr))  s->zr = 0;
	if (!finite(s->zlr)) s->zlr = 0;
	if (!finite(s->zll)) s->zll = 0;
	if (!finite(s->zrr)) s->zrr = 0;

	s->zlr += 1e-10f;
	s->zll += 1e-10f;
	s->zrr += 1e-10f;
}

float stc_read (struct stcorr const * const s) {
	return s->zlr / sqrtf (s->zll * s->zrr + 1e-10f);
}


typedef struct {
	float* input[2];
	float* output[2];

	float* spec[FILTER_COUNT];
	float* maxf[FILTER_COUNT];

	float  max_f[FILTER_COUNT];
	struct FilterBank flt_l[FILTER_COUNT];
	struct FilterBank flt_r[FILTER_COUNT];
	struct stcorr cor[FILTER_COUNT];

	double rate;

	float  omega;
	float sw1;
	float sw2;

} LV2mphase;

/******************************************************************************
 * LV2 callbacks
 */

static LV2_Handle
multiphase_instantiate(
		const LV2_Descriptor*     descriptor,
		double                    rate,
		const char*               bundle_path,
		const LV2_Feature* const* features)
{
	if (!strcmp(descriptor->URI, MTR_URI "multiphase")
			|| !strcmp(descriptor->URI, MTR_URI "multiphase_gtk"))
	{
		;
	}
	else { return NULL; }

	LV2mphase* self = (LV2mphase*)calloc(1, sizeof(LV2mphase));
	if (!self) return NULL;

	self->rate = rate;

	// 1.0 - e^(-2.0 * π * v / 48000)
	self->omega = 1.0f - expf(-2.0 * M_PI / rate);

	/* filter-frequencies */
	const double f_r = 1000;
	const double b = 3;
	const double f1f = pow(2, -1. / (2. * b));
	const double f2f = pow(2,  1. / (2. * b));

	for (uint32_t i=0; i < FILTER_COUNT; ++i) {

		// TODO: THINK:  instead of bandpass filtering,
		// use differences of low-pass filtered phases
		const int x = i - 16;
		const double f_m = pow(2, x / b) * f_r;
		const double f_1 = f_m * f1f;
		const double f_2 = f_m * f2f;
		const double bw  = f_2 - f_1;
#ifdef DEBUG_SPECTR
		printf("--F %2d (%3d): f:%9.2fHz b:%9.2fHz (%9.2fHz -> %9.2fHz)\n",i, x, f_m, bw, f_1, f_2);
#endif
		self->max_f[i] = 0;
		bandpass_setup(&self->flt_l[i], self->rate, f_m, bw, 2);
		bandpass_setup(&self->flt_r[i], self->rate, f_m, bw, 2);
		memset(&self->cor[i], 0, sizeof(struct stcorr));
	}

  self->sw1 = 6.28f * 2e3f / self->rate;
  self->sw2 = 1 / (.5 * self->rate);

	return (LV2_Handle)self;
}

static void
multiphase_connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	LV2mphase* self = (LV2mphase*)instance;
	switch (port) {
	case MF_INPUT0:
		self->input[0] = (float*) data;
		break;
	case MF_OUTPUT0:
		self->output[0] = (float*) data;
		break;
	case MF_INPUT1:
		self->input[1] = (float*) data;
		break;
	case MF_OUTPUT1:
		self->output[1] = (float*) data;
		break;
	default:
		if (port >= 0 && port < 30) {
			self->spec[port] = (float*) data;
		}
		if (port >= 30 && port < 60) {
			self->maxf[port-30] = (float*) data;
		}
		break;
	}
}

static void
multiphase_run(LV2_Handle instance, uint32_t n_samples)
{
	LV2mphase* self = (LV2mphase*)instance;
	float* inL = self->input[0];
	float* inR = self->input[1];

	/* localize variables */
	const float _w1 = self->sw1;
	const float _w2 = self->sw2;
	float max_f[FILTER_COUNT];
	const float omega  = self->omega;
	struct FilterBank *flt_l[FILTER_COUNT];
	struct FilterBank *flt_r[FILTER_COUNT];

	for(int i=0; i < FILTER_COUNT; ++i) {
		max_f[i] = self->max_f[i];
		flt_l[i] = &self->flt_l[i];
		flt_r[i] = &self->flt_r[i];
	}

	/* .. and go */
	for (uint32_t j = 0 ; j < n_samples; ++j) {
		const float L = *(inL++);
		const float R = *(inR++);

		for(int i = 0; i < FILTER_COUNT; ++i) {
			const float fl = bandpass_process(flt_l[i], L);
			const float fr = bandpass_process(flt_r[i], R);
			stc_proc_one(&self->cor[i], fl, fr, _w1, _w2);

			const float v = fl * fl + fr * fr;
			const float s = v / 2;
			max_f[i] += omega * (s - max_f[i]);
		}
	}

	/* copy back variables and assign value */
	for(int i=0; i < FILTER_COUNT; ++i) {
		stc_proc_end(&self->cor[i]);

		if (!finite(max_f[i])) max_f[i] = 0;
		for (uint32_t j=0; j < flt_l[i]->filter_stages; ++j) {
			if (!finite(flt_l[i]->f[j].z[0])) flt_l[i]->f[j].z[0] = 0;
			if (!finite(flt_l[i]->f[j].z[1])) flt_l[i]->f[j].z[1] = 0;
			if (!finite(flt_r[i]->f[j].z[0])) flt_r[i]->f[j].z[0] = 0;
			if (!finite(flt_r[i]->f[j].z[1])) flt_r[i]->f[j].z[1] = 0;
		}

		self->max_f[i] = max_f[i];
		const float mx = sqrtf(2. * max_f[i]);

		*(self->maxf[i]) = mx > .001f ? 20.0 * log10f(mx) : -60.0;
		*(self->spec[i]) = stc_read(&self->cor[i]);
	}

	if (self->input[0] != self->output[0]) {
		memcpy(self->output[0], self->input[0], sizeof(float) * n_samples);
	}
	if (self->input[1] != self->output[1]) {
		memcpy(self->output[1], self->input[1], sizeof(float) * n_samples);
	}
}

static void
multiphase_cleanup(LV2_Handle instance)
{
	free(instance);
}

#define MPHASEDESC(ID, NAME) \
static const LV2_Descriptor descriptor ## ID = { \
	MTR_URI NAME, \
	multiphase_instantiate, \
	multiphase_connect_port, \
	NULL, \
	multiphase_run, \
	NULL, \
	multiphase_cleanup, \
	extension_data \
};

MPHASEDESC(MultiPhase, "multiphase");
MPHASEDESC(MultiPhaseGtk, "multiphase_gtk");
