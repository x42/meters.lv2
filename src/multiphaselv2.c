/* meter.lv2 - Phase Wheel
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
 * 360deg Stereo Phase Calculation
 */

typedef enum {
	MF_PHASE    = 60,
	MF_GAIN     = 61,
	MF_CUTOFF   = 62,
	MF_INPUT0   = 63,
	MF_OUTPUT0  = 64,
	MF_INPUT1   = 65,
	MF_OUTPUT1  = 66,
} MFPortIndex;

struct stcorr {
	float zlr, ylr;
	float zll, yll;
	float zrr;

	float _w;

	float *_yl;
	uint32_t yp;
	uint32_t ym;
	float    ps;
};

static inline void stc_proc_one (struct stcorr *s, const float pl, const float pr) {
	const float _w = s->_w;
	const float _y = s->_yl[ s->yp ];

	s->ylr += _w * (_y * pr - s->ylr);
	s->yll += _w * (_y * _y - s->yll);

	s->zlr += _w * (pl * pr - s->zlr);
	s->zll += _w * (pl * pl - s->zll);
	s->zrr += _w * (pr * pr - s->zrr);

	s->_yl[ s->yp ] = pl;
	s->yp = (s->yp + 1) % s->ym;
}

static inline void stc_proc_end (struct stcorr *s) {
	if (!finite(s->ylr)) s->ylr = 0;
	if (!finite(s->yll)) s->yll = 0;

	if (!finite(s->zlr)) s->zlr = 0;
	if (!finite(s->zll)) s->zll = 0;
	if (!finite(s->zrr)) s->zrr = 0;

	s->ylr += 1e-10f;
	s->yll += 1e-10f;
	s->zlr += 1e-10f;
	s->zll += 1e-10f;
	s->zrr += 1e-10f;
}

float stc_read (struct stcorr const * const s) {
	const float p0 = .25 * s->zlr / sqrtf(s->zll * s->zrr + 1e-10f);
	const float p1 = .25 * s->ylr / sqrtf(s->yll * s->zrr + 1e-10f);
	float rv;

	// TODO simplify quadrant calc, use 1/4 shift diff
	if (p0 >= 0 && p1 >= 0) {
		rv =   0 + ( p0 - p1);
	} else 
	if (p0 <  0 && p1 >= 0) {
		rv = -.5 + ( p0 + p1);
	} else
	if (p0 >= 0 && p1 <  0) {
		rv =  .5 + (-p0 - p1);
	} else {
	//  p0 <  0 && p1 <  0
		rv =  -1 + (-p0 + p1);
	}
	return  rv - s->ps;
}

/******************************************************************************
 * LV2 spec
 */

#define FILTER_COUNT (30)


typedef struct {
	float* input[2];
	float* output[2];
	float* p_gain;
	float* p_phase;

	float* spec[FILTER_COUNT];
	float* maxf[FILTER_COUNT];

	float  max_f[FILTER_COUNT];
	struct FilterBank flt_l[FILTER_COUNT];
	struct FilterBank flt_r[FILTER_COUNT];
	struct stcorr cor[FILTER_COUNT];

	Stcorrdsp *stcor;

	double rate;
	float  omega;
	float  gain_db;
	float  gain_coeff;

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

	self->gain_db = 0;
	self->gain_coeff = 1.0;

	self->stcor = new Stcorrdsp();
	self->stcor->init(rate, 2e3f, 0.3f);

	// 1.0 - e^(-2.0 * π * v / 48000)
	self->omega = 1.0f - expf(-2.0 * M_PI * 5 / rate);

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
		bandpass_setup(&self->flt_l[i], self->rate, f_m, bw, 4);
		bandpass_setup(&self->flt_r[i], self->rate, f_m, bw, 4);
		memset(&self->cor[i], 0, sizeof(struct stcorr));

		self->cor[i]._w = 1.0f - expf(-0.01 * M_PI * f_m / self->rate);
		self->cor[i].ym = ceil(self->rate / f_m / 4.0);
		self->cor[i].ps = self->cor[i].ym / (self->rate / f_m);
		self->cor[i]._yl = (float*) calloc(self->cor[i].ym, sizeof(float));
	}

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
	case MF_GAIN:
		self->p_gain = (float*) data;
		break;
	case MF_PHASE:
		self->p_phase = (float*) data;
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

	if (*self->p_gain != self->gain_db) {
		self->gain_db = *self->p_gain;
		self->gain_coeff = powf(10, .05 * self->gain_db);
	}

	/* localize variables */
	float max_f[FILTER_COUNT];
	const float omega  = self->omega;
	const float gain   = self->gain_coeff;
	struct FilterBank *flt_l[FILTER_COUNT];
	struct FilterBank *flt_r[FILTER_COUNT];

	for(int i=0; i < FILTER_COUNT; ++i) {
		max_f[i] = self->max_f[i];
		flt_l[i] = &self->flt_l[i];
		flt_r[i] = &self->flt_r[i];
	}

	/* calculate total phase */
	self->stcor->process(self->input[0], self->input[1] , n_samples);
	*self->p_phase = self->stcor->read();

	/* per frequency-band phase & amplitude */
	for (uint32_t j = 0 ; j < n_samples; ++j) {
		const float L = *(inL++) * gain;
		const float R = *(inR++) * gain;

		for(int i = 0; i < FILTER_COUNT; ++i) {
			const float fl = bandpass_process(flt_l[i], L);
			const float fr = bandpass_process(flt_r[i], R);

			stc_proc_one(&self->cor[i], fl, fr);

			const float v = (fl * fl + fr * fr) / 2.f;
			max_f[i] += omega * (v - max_f[i]);
		}
	}

	/* copy back variables and assign value */
	for(int i=0; i < FILTER_COUNT; ++i) {
		stc_proc_end(&self->cor[i]);

		for (uint32_t j=0; j < flt_l[i]->filter_stages; ++j) {
			if (!finite(flt_l[i]->f[j].z[0])) flt_l[i]->f[j].z[0] = 0;
			if (!finite(flt_l[i]->f[j].z[1])) flt_l[i]->f[j].z[1] = 0;
			if (!finite(flt_r[i]->f[j].z[0])) flt_r[i]->f[j].z[0] = 0;
			if (!finite(flt_r[i]->f[j].z[1])) flt_r[i]->f[j].z[1] = 0;
		}

		if (!finite(max_f[i])) max_f[i] = 0;
		const float mx = sqrtf(2.f * max_f[i]);
		self->max_f[i] = max_f[i];
		max_f[i] += 1e-10;

		*(self->maxf[i]) = mx > .001f ? (mx > 1.0 ? 0 : 20.0 * log10f(mx)) : -60.0;
		*(self->spec[i]) = stc_read(&self->cor[i]);
	}

	/* forward data to outputs -- IFF not in-place */
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
	LV2mphase* self = (LV2mphase*)instance;
	for (uint32_t i=0; i < FILTER_COUNT; ++i) {
		free(self->cor[i]._yl);
	}
	delete self->stcor;
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
