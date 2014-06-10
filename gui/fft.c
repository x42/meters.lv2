/* FFT analysis - spectrogram
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
#include <sys/types.h>
#include <pthread.h>
#include <fftw3.h>

#ifndef MIN
#define MIN(A,B) ( (A) < (B) ? (A) : (B) )
#endif

static pthread_mutex_t fftw_planner_lock = PTHREAD_MUTEX_INITIALIZER;

/******************************************************************************
 * internal FFT abstraction
 */
struct FFTAnalysis {
	uint32_t window_size;
	uint32_t data_size;
	double rate;
	double freq_per_bin;
	double phasediff_step;
	float *hann_window;
	float *fft_in;
	float *fft_out;
	float *power;
	float *phase;
	float *phase_h;
	fftwf_plan fftplan;

	float *ringbuf;
	uint32_t rboff;
	uint32_t smps;
	uint32_t sps;
	uint32_t step;
	double phasediff_bin;
};

/******************************************************************************
 * internal private functions
 */
static float * ft_hann_window(struct FFTAnalysis *ft) {
	if (ft->hann_window) return ft->hann_window;
	ft->hann_window = (float *) malloc(sizeof(float) * ft->window_size);
	double sum = 0.0;

	for (uint32_t i=0; i < ft->window_size; i++) {
		ft->hann_window[i] = 0.5f - (0.5f * (float) cos(2.0f * M_PI * (float)i / (float)(ft->window_size)));
		sum += ft->hann_window[i];
	}
	const double isum = 2.0 / sum;
	for (uint32_t i=0; i < ft->window_size; i++) {
		ft->hann_window[i] *= isum;
	}

	return ft->hann_window;
}

static void ft_analyze(struct FFTAnalysis *ft) {
	float *window = ft_hann_window(ft);
	for (uint32_t i = 0; i < ft->window_size; i++) {
		ft->fft_in[i] *= window[i];
	}

	fftwf_execute(ft->fftplan);

	memcpy(ft->phase_h, ft->phase, sizeof(float) * ft->data_size);
	ft->power[0] = ft->fft_out[0] * ft->fft_out[0];
	ft->phase[0] = 0;

#define FRe (ft->fft_out[i])
#define FIm (ft->fft_out[ft->window_size-i])
	for (uint32_t i = 1; i < ft->data_size - 1; ++i) {
		ft->power[i] = (FRe * FRe) + (FIm * FIm);
		ft->phase[i] = atan2f(FIm, FRe);
	}
#undef FRe
#undef FIm
}

/******************************************************************************
 * public API (static for direct source inclusion)
 */
#ifndef FFTX_FN_PREFIX
#define FFTX_FN_PREFIX static
#endif

FFTX_FN_PREFIX
void fftx_reset(struct FFTAnalysis *ft) {
	for (uint32_t i = 0; i < ft->data_size; ++i) {
		ft->power[i] = 0;
		ft->phase[i] = 0;
		ft->phase_h[i] = 0;
	}
	for (uint32_t i = 0; i < ft->window_size; ++i) {
		ft->ringbuf[i] = 0;
		ft->fft_out[i] = 0;
	}
	ft->rboff = 0;
	ft->smps = 0;
	ft->step = 0;
}

FFTX_FN_PREFIX
void fftx_init(struct FFTAnalysis *ft, uint32_t window_size, double rate, double fps) {
	ft->rate        = rate;
	ft->window_size = window_size;
	ft->data_size   = window_size / 2;
	ft->hann_window = NULL;
	ft->rboff = 0;
	ft->smps = 0;
	ft->step = 0;
	ft->sps = (fps > 0) ? ceil(rate / fps) : 0;
	ft->freq_per_bin = ft->rate / ft->data_size / 2.f;
	ft->phasediff_step = M_PI / ft->data_size;
	ft->phasediff_bin = 0;

	ft->ringbuf = (float *) malloc(window_size * sizeof(float));
	ft->fft_in  = (float *) fftwf_malloc(sizeof(float) * window_size);
	ft->fft_out = (float *) fftwf_malloc(sizeof(float) * window_size);
	ft->power   = (float *) malloc(ft->data_size * sizeof(float));
	ft->phase   = (float *) malloc(ft->data_size * sizeof(float));
	ft->phase_h = (float *) malloc(ft->data_size * sizeof(float));

	fftx_reset(ft);

	pthread_mutex_lock(&fftw_planner_lock);
	ft->fftplan = fftwf_plan_r2r_1d(window_size, ft->fft_in, ft->fft_out, FFTW_R2HC, FFTW_MEASURE);
	pthread_mutex_unlock(&fftw_planner_lock);
}

FFTX_FN_PREFIX
void fftx_free(struct FFTAnalysis *ft) {
	if (!ft) return;
	pthread_mutex_lock(&fftw_planner_lock);
	fftwf_destroy_plan(ft->fftplan);
	pthread_mutex_unlock(&fftw_planner_lock);
	free(ft->hann_window);
	free(ft->ringbuf);
	free(ft->fft_in);
	free(ft->fft_out);
	free(ft->power);
	free(ft->phase);
	free(ft->phase_h);
	free(ft);
}

static
int _fftx_run(struct FFTAnalysis *ft,
		const uint32_t n_samples, float const * const data)
{
	assert(n_samples <= ft->window_size);

	float * const f_buf = ft->fft_in;
	float * const r_buf = ft->ringbuf;

	const uint32_t n_off = ft->rboff;
	const uint32_t n_siz = ft->window_size;
	const uint32_t n_old = n_siz - n_samples;

	/* copy new data into ringbuffer and fft-buffer
	 * TODO: use memcpy
	 */
	for (uint32_t i = 0; i < n_samples; ++i) {
		r_buf[ (i + n_off) % n_siz ]  = data[i];
		f_buf[n_old + i] = data[i];
	}

	ft->rboff = (ft->rboff + n_samples) % n_siz;
#if 1
	ft->smps += n_samples;
	if (ft->smps < ft->sps) {
		return -1;
	}
	ft->step = ft->smps;
	ft->smps = 0;
#else
	ft->step = n_samples;
#endif

	/* copy samples from ringbuffer into fft-buffer */
	const uint32_t p0s = (n_off + n_samples) % n_siz;
	if (p0s + n_old >= n_siz) {
		const uint32_t n_p1 = n_siz - p0s;
		const uint32_t n_p2 = n_old - n_p1;
		memcpy(f_buf, &r_buf[p0s], sizeof(float) * n_p1);
		memcpy(&f_buf[n_p1], &r_buf[0], sizeof(float) * n_p2);
	} else {
		memcpy(&f_buf[0], &r_buf[p0s], sizeof(float) * n_old);
	}

	/* ..and analyze */
	ft_analyze(ft);
	ft->phasediff_bin = ft->phasediff_step * (double)ft->step;
	return 0;
}

FFTX_FN_PREFIX
int fftx_run(struct FFTAnalysis *ft,
		const uint32_t n_samples, float const * const data)
{
	if (n_samples <= ft->window_size) {
		return _fftx_run(ft, n_samples, data);
	}

	int rv = -1;
	uint32_t n = 0;
	while (n < n_samples) {
		uint32_t step = MIN(ft->window_size, n_samples - n);
		if (!_fftx_run(ft, step, &data[n])) rv = 0;
		n += step;
	}
	return rv;
}


/*****************************************************************************
 * convenient access functions
 */

FFTX_FN_PREFIX
uint32_t fftx_bins(struct FFTAnalysis *ft) {
	 return ft->data_size;
}

FFTX_FN_PREFIX
inline float fast_log2 (float val) {
	union {float f; int i;} t;
	t.f = val;
	int * const    exp_ptr =  &t.i;
	int            x = *exp_ptr;
	const int      log_2 = ((x >> 23) & 255) - 128;
	x &= ~(255 << 23);
	x += 127 << 23;
	*exp_ptr = x;
	val = ((-1.0f/3) * t.f + 2) * t.f - 2.0f/3;
	return (val + log_2);
}

FFTX_FN_PREFIX
inline float fast_log (const float val) {
  return (fast_log2 (val) * 0.69314718f);
}

FFTX_FN_PREFIX
inline float fast_log10 (const float val) {
  return fast_log2(val) / 3.312500f;
}

FFTX_FN_PREFIX
inline float fftx_power_to_dB(float a) {
	/* 10 instead of 20 because of squared signal -- no sqrt(powerp[]) */
	return a > 1e-12 ? 10.0 * fast_log10(a) : -INFINITY;
}

FFTX_FN_PREFIX
float fftx_power_at_bin(struct FFTAnalysis *ft, const int b) {
	return (fftx_power_to_dB(ft->power[b]));
}

FFTX_FN_PREFIX
float fftx_freq_at_bin(struct FFTAnalysis *ft, const int b) {
	/* calc phase: difference minus expected difference */
	float phase = ft->phase[b] - ft->phase_h[b] - (float) b * ft->phasediff_bin;
	/* clamp to -M_PI .. M_PI */
	int over = phase / M_PI;
	over += (over >= 0) ? (over&1) : -(over&1);
	phase -= M_PI*(float)over;
	/* scale according to overlap */
	phase *= (ft->data_size / ft->step) / M_PI;
	return ft->freq_per_bin * ((float) b + phase);
}
