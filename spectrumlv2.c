/* meter.lv2
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

/* static functions to be included in meters.cc
 *
 * broken out spectrum analyzer related LV2 functions
 */

/******************************************************************************
 * bandpass filter
 */

struct FilterParam {
	float x [3];
	float y0[3];
	float y1[3];
	float y2[3];
	float c [5];
};

static void bandpass_init (struct FilterParam *f, double rate, double freq, double bw) {
	for(int i=0; i < 3; ++i) {
		f->x[i]  = f->y0[i] = 0.0;
		f->y1[i] = f->y2[i] = 0.0;
	}

	if(freq >= rate/2.0) {
		f->c[0] = f->c[1] = f->c[2] = f->c[3] = f->c[4] = 0.0;
		return;
	}

	/* adjust bandwidth to below nyquist */
	if( (1.0 + bw) * freq > rate/2.0) {
		bw = ( bw + ((rate / (2.0 * freq)) - 1.0)  ) / 2.0;
		//printf("Adjusted bandwidth for band %f -> %f\n", freq, bw);
	}

	const double w0 = 2.0 * M_PI * freq / rate;
	const double alpha = sin(w0) * sinh((log(2)/2) * bw * (w0/sin(w0)));

	const double b0 =  alpha;
	const double b1 =  0;
	const double b2 = -alpha;

	const double a0 =  1.0 + alpha;
	const double a1 = -2.0 * cos(w0);
	const double a2 =  1.0 - alpha;

	f->c[0] = b0 / a0;
	f->c[1] = b1 / a0;
	f->c[2] = b2 / a0;
	f->c[3] = a1 / a0;
	f->c[4] = a2 / a0;

	return;
}

static float bandpass_proc (struct FilterParam *f, const float in) {
#if 1
	for(int i=0; i < 2; ++i) {
		f->x[i]  = f->x[i+1];
		f->y0[i] = f->y0[i+1];
		f->y1[i] = f->y1[i+1];
		f->y2[i] = f->y2[i+1];
	}
#else
		f->x [0] = f->x [1];
		f->y0[0] = f->y0[1];
		f->y1[0] = f->y1[1];
		f->y2[0] = f->y2[1];

		f->x [1] = f->x [2];
		f->y0[1] = f->y0[2];
		f->y1[1] = f->y1[2];
		f->y2[1] = f->y2[2];
#endif

#define CONVOLV(INP,OUT) \
	f->OUT[2] = f->c[0]*f->INP[2] + f->c[1]*f->INP[1] + f->c[2]*f->INP[0] \
	                              - f->c[3]*f->OUT[1] - f->c[4]*f->OUT[0] \
	                                                  + 1e-12;
	f->x[2] = in;
	CONVOLV(x,y0)
	f->y1[2] = f->y0[2];
	CONVOLV(y1,y2)
	return f->y2[2];
}

/******************************************************************************
 * LV2 spec
 */

#define FILTER_COUNT (31)
#define BP_Q (.33)

typedef enum {
	SA_INPUT0   = 0,
	SA_OUTPUT0  = 1,
	SA_INPUT1   = 2,
	SA_OUTPUT1  = 3,
	SA_GAIN     = 4,
	SA_ATTACK   = 36,
	SA_DECAY    = 37,
} SAPortIndex;

typedef struct {
	float* input[2];
	float* output[2];

	float* gain;
	float* spec[FILTER_COUNT];
	float* attack_p;
	float* decay_p;

	double rate;
	float attack, attack_h;
	float decay, decay_h;

	float  spec_f[FILTER_COUNT];
	struct FilterParam flt[FILTER_COUNT];

} LV2spec;

static const float freq_table [FILTER_COUNT] = {
	   20.0,    25.0,    31.5,
	   40.0,    50.0,    63.0, 80.0,
	  100.0,   125.0,   160.0,
	  200.0,   250.0,   315.0,
	  400.0,   500.0,   630.0, 800.0,
	 1000.0,  1250.0,  1600.0,
	 2000.0,  2500.0,  3150.0,
	 4000.0,  5000.0,  6300.0, 8000.0,
	10000.0, 12500.0, 16000.0,
	20000.0
};


/******************************************************************************
 * LV2 callbacks
 */

static LV2_Handle
spectrum_instantiate(
		const LV2_Descriptor*     descriptor,
		double                    rate,
		const char*               bundle_path,
		const LV2_Feature* const* features)
{
	if (strcmp(descriptor->URI, MTR_URI "spectrum")) {
		return NULL;
	}

	LV2spec* self = (LV2spec*)calloc(1, sizeof(LV2spec));
	if (!self) return NULL;

	self->attack_h = 15.0;
	self->decay_h = .5;
	self->rate = rate;

	// 1.0 - e^(-2.0 * π * v / 48000)
	self->attack = 1.0f - expf(-2.0 * M_PI * self->attack_h / rate);
	self->decay  = 1.0f - expf(-2.0 * M_PI * self->decay_h / rate);

	for (int i = 0; i < FILTER_COUNT; ++i) {
		self->spec_f[i] = 0;
		bandpass_init(&self->flt[i],  self->rate,  freq_table[i], BP_Q);
	}

	return (LV2_Handle)self;
}

static void
spectrum_connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	LV2spec* self = (LV2spec*)instance;
	switch (port) {
	case SA_INPUT0:
		self->input[0] = (float*) data;
		break;
	case SA_OUTPUT0:
		self->output[0] = (float*) data;
		break;
	case SA_INPUT1:
		self->input[1] = (float*) data;
		break;
	case SA_OUTPUT1:
		self->output[1] = (float*) data;
		break;
	case SA_GAIN:
		self->gain = (float*) data;
		break;
	case SA_ATTACK:
		self->attack_p = (float*) data;
		break;
	case SA_DECAY:
		self->decay_p = (float*) data;
		break;
	default:
		if (port > 4 && port < 36) {
			self->spec[port-5] = (float*) data;
		}
		break;
	}
}

static void
spectrum_run(LV2_Handle instance, uint32_t n_samples)
{
	LV2spec* self = (LV2spec*)instance;
	float* inL = self->input[0];
	float* inR = self->input[1];

	if (self->attack_h != *self->attack_p) {
		self->attack_h = *self->attack_p;
		float v = self->attack_h;
		if (v < 1.0) v = 1.0;
		if (v > 1000.0) v = 1000.0;
		self->attack = 1.0f - expf(-2.0 * M_PI * v / self->rate);
	}
	if (self->decay_h != *self->decay_p) {
		self->decay_h = *self->decay_p;
		float v = self->decay_h;
		if (v < 0.01) v = 0.01;
		if (v > 15.0) v = 15.0;
		self->decay  = 1.0f - expf(-2.0 * M_PI * v / self->rate);
	}
	/* localize variables */
	float spec_f[FILTER_COUNT];
	const float attack = self->attack > self->decay ? self->attack : self->decay;
	const float decay  = self->decay;
	const float gain   = *self->gain;
	struct FilterParam *flt[FILTER_COUNT];

	for(int i=0; i < FILTER_COUNT; ++i) {
		spec_f[i] = self->spec_f[i];
		flt[i] = &self->flt[i];
	}

	/* .. and go */
	for (uint32_t j = 0 ; j < n_samples; ++j) {
		const float L = *(inL++);
		const float R = *(inR++);
		const float in  = gain * (L + R) / 2.0f;
				
		for(int i = 0; i < FILTER_COUNT; ++i) {
			const float v = fabsf(bandpass_proc(flt[i], in));
			spec_f[i] +=  10e-20 + (v > spec_f[i] ? attack * (v - spec_f[i]) : decay * (v - spec_f[i]));
		}
	}

	/* copy back variables */
	for(int i=0; i < FILTER_COUNT; ++i) {
		self->spec_f[i] = spec_f[i];
		spec_f[i] *= 1.0592f;
		*(self->spec[i]) = spec_f[i] > .000316f ? 20.0 * log10f(spec_f[i]) : -70.0;
	}

	if (self->input[0] != self->output[0]) {
		memcpy(self->output[0], self->input[0], sizeof(float) * n_samples);
	}
	if (self->input[1] != self->output[1]) {
		memcpy(self->output[1], self->input[1], sizeof(float) * n_samples);
	}
}

static void
spectrum_cleanup(LV2_Handle instance)
{
	free(instance);
}

static const LV2_Descriptor descriptorSpectrum = {
	MTR_URI "spectrum",
	spectrum_instantiate,
	spectrum_connect_port,
	NULL,
	spectrum_run,
	NULL,
	spectrum_cleanup,
	extension_data
};
