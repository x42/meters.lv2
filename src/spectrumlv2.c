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

#include "spectr.c"

/******************************************************************************
 * LV2 spec
 */

#define FILTER_COUNT (30)

typedef enum {
	SA_INPUT0   = 0,
	SA_OUTPUT0  = 1,
	SA_INPUT1   = 2,
	SA_OUTPUT1  = 3,
	SA_GAIN     = 4,
	SA_ATTACK   = 35,
	SA_DECAY    = 36,
} SAPortIndex;

typedef struct {
	float* input[2];
	float* output[2];

	float* gain;
	float gain_h;
	float* spec[FILTER_COUNT];
	float* attack_p;
	float* decay_p;

	double rate;
	float attack, attack_h;
	float decay, decay_h;

	float  spec_f[FILTER_COUNT];
	struct FilterBank flt[FILTER_COUNT];

} LV2spec;

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
	if (strcmp(descriptor->URI, MTR_URI "spectr30") && strcmp(descriptor->URI, MTR_URI "spectr30_gtk")) {
		return NULL;
	}

	LV2spec* self = (LV2spec*)calloc(1, sizeof(LV2spec));
	if (!self) return NULL;

	self->attack_h = 15.0;
	self->decay_h = .5;
	self->gain_h = 1.0;
	self->rate = rate;

	// 1.0 - e^(-2.0 * π * v / 48000)
	self->attack = 1.0f - expf(-2.0 * M_PI * self->attack_h / rate);
	self->decay  = 1.0f - expf(-2.0 * M_PI * self->decay_h / rate);

	/* filter-frequencies */
	const double f_r = 1000;
	const double b = 3;
	const double f1f = pow(2, -1. / (2. * b));
	const double f2f = pow(2,  1. / (2. * b));

	for (uint32_t i=0; i < FILTER_COUNT; ++i) {
		const int x = i - 16;
		const double f_m = pow(2, x / b) * f_r;
		const double f_1 = f_m * f1f;
		const double f_2 = f_m * f2f;
		const double bw  = f_2 - f_1;
		printf("--F %2d (%3d): f:%9.2fHz b:%9.2fHz (%9.2fHz -> %9.2fHz)\n",i, x, f_m, bw, f_1, f_2);
		self->spec_f[i] = 0;
		bandpass_setup(&self->flt[i], self->rate, f_m, bw);
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

	/* calculate time-constants when they're changed,
	 * no-need to smoothen then for the visual display
	 */
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
	struct FilterBank *flt[FILTER_COUNT];

	float mx [FILTER_COUNT];
	for(int i=0; i < FILTER_COUNT; ++i) {
		spec_f[i] = self->spec_f[i];
		flt[i] = &self->flt[i];
		mx[i] = 0;
	}

	if (self->gain_h != gain) {
		self->gain_h = gain;
		for(int i = 0; i < FILTER_COUNT; ++i) {
			spec_f[i] = 0;
		}
	}

	/* .. and go */
	for (uint32_t j = 0 ; j < n_samples; ++j) {
		const float L = *(inL++);
		const float R = *(inR++);
		const float in  = gain * (L + R) / 2.0f;
				
		for(int i = 0; i < FILTER_COUNT; ++i) {
			// TODO use RMS and low-pass filter
			const float v = fabsf(bandpass_process(flt[i], in));
			spec_f[i] += v > spec_f[i] ? attack * (v - spec_f[i]) : decay * (v - spec_f[i]);
			if (spec_f[i] > mx[i]) mx[i] = spec_f[i];
		}
	}

	/* copy back variables and assign value */
	for(int i=0; i < FILTER_COUNT; ++i) {
		if (!finite(spec_f[i])) spec_f[i] = 0;
		else if (spec_f[i] > 100) spec_f[i] = 100;
		else if (spec_f[i] < 0) spec_f[i] = 0;

		for (uint32_t j=0; j < flt[i]->filter_stages; ++j) {
			if (!finite(flt[i]->f[j].z[0])) flt[i]->f[j].z[0] = 0;
			if (!finite(flt[i]->f[j].z[1])) flt[i]->f[j].z[1] = 0;
		}

		self->spec_f[i] = spec_f[i] + 10e-12 ;
		*(self->spec[i]) = mx[i] > .000316f ? 20.0 * log10f(mx[i]) : -70.0;
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
	MTR_URI "spectr30",
	spectrum_instantiate,
	spectrum_connect_port,
	NULL,
	spectrum_run,
	NULL,
	spectrum_cleanup,
	extension_data
};

static const LV2_Descriptor descriptorSpectrumGtk = {
	MTR_URI "spectr30_gtk",
	spectrum_instantiate,
	spectrum_connect_port,
	NULL,
	spectrum_run,
	NULL,
	spectrum_cleanup,
	extension_data
};
