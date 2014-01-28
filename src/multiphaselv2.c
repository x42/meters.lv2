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

#define NUM_BANDS (40)

typedef enum {
	MF_PHASE = (NUM_BANDS + NUM_BANDS),
	MF_GAIN,
	MF_CUTOFF,
	MF_INPUT0,
	MF_OUTPUT0,
	MF_INPUT1,
	MF_OUTPUT1
} MFPortIndex;

/******************************************************************************
 * 360deg Stereo Phase Calculation
 */

struct stcorr {
	float zlr, zll, zrr;
	float ylr, yrl; // 1/4 phase shift
	float xlr, xrl; // 3/4 phase shift

	float *dll, *dlr; // delay-line data
	uint32_t dls;     // delay-line length
	uint32_t yp, xp;  // read-pointers
};

static inline void stc_proc_one (struct stcorr *s, const float _w, const float pl, const float pr) {
	const float yl = s->dll[ s->yp ];
	const float yr = s->dlr[ s->yp ];
	const float xl = s->dll[ s->xp ];
	const float xr = s->dlr[ s->xp ];

	s->ylr += _w * (yl * pr - s->ylr);
	s->yrl += _w * (yr * pl - s->yrl);
	s->xlr += _w * (xl * pr - s->xlr);
	s->xrl += _w * (xr * pl - s->xrl);

	s->zlr += _w * (pl * pr - s->zlr);
	s->zll += _w * (pl * pl - s->zll);
	s->zrr += _w * (pr * pr - s->zrr);

	s->dll[ s->xp ] = pl;
	s->dlr[ s->xp ] = pr;
	s->yp = (s->yp + 1) % s->dls;
	s->xp = (s->xp + 1) % s->dls;
}

static inline void stc_proc_end (struct stcorr *s) {
	/* protect against denormals */
	if (!finite(s->ylr)) s->ylr = 0;
	if (!finite(s->yrl)) s->ylr = 0;
	if (!finite(s->xlr)) s->xlr = 0;
	if (!finite(s->xrl)) s->xlr = 0;

	if (!finite(s->zlr)) s->zlr = 0;
	if (!finite(s->zll)) s->zll = 0;
	if (!finite(s->zrr)) s->zrr = 0;

	s->ylr += 1e-10f;
	s->yrl += 1e-10f;
	s->xlr += 1e-10f;
	s->xrl += 1e-10f;

	s->zlr += 1e-10f;
	s->zll += 1e-10f;
	s->zrr += 1e-10f;
}

float stc_read (struct stcorr const * const s) {
	const float nrm = 1.0 / sqrtf(s->zll * s->zrr + 1e-10f);
	const float p0 = s->zlr * nrm;
	const float pl = (s->ylr - s->yrl - s->xlr + s->xrl) * .5 * nrm;
	float rv;

	/* quadrant */
	if (fabsf(p0) < .005 && fabsf(pl) < .005) {
		/* no correlation */
		return -100;
	} else
	if (p0 >= 0 && pl >= 0) {
		rv =  -1 + ( p0 - pl);
	} else
	if (p0 <  0 && pl >= 0) {
		rv =  -3 + ( p0 + pl);
	} else
	if (p0 >= 0 && pl <  0) {
		rv =   1 + (-p0 - pl);
	} else {
	//  p0 <  0 && pl <  0
		rv =   3 + (-p0 + pl);
	}
	return  rv * .25;
}

/******************************************************************************
 * LV2 spec
 */


typedef struct {
	float* input[2];
	float* output[2];
	float* p_gain;
	float* p_phase;

	float* spec[NUM_BANDS];
	float* maxf[NUM_BANDS];

	float  max_f[NUM_BANDS];
	struct FilterBank flt_l[NUM_BANDS];
	struct FilterBank flt_r[NUM_BANDS];
	struct stcorr cor[NUM_BANDS];

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
	self->omega = 1.0f - expf(-2.f * M_PI * 7.5 / rate);

	/* filter-frequencies */
	const double f_r = 1000;
	const double b = 4;
	const double f1f = pow(2, -1. / (2. * b));
	const double f2f = pow(2,  1. / (2. * b));

	// TODO: prototype FFT based approach instead of discrete band filters

	for (uint32_t i=0; i < NUM_BANDS; ++i) {
		const int x = i - 22;
		const double f_m = pow(2, x / b) * f_r;
		const double f_1 = f_m * f1f;
		const double f_2 = f_m * f2f;
		const double bw  = f_2 - f_1;
#ifdef DEBUG_SPECTR
		printf("--F %2d (%3d): f:%9.2fHz b:%9.2fHz (%9.2fHz -> %9.2fHz) 1/4spl:%.2f\n",i, x, f_m, bw, f_1, f_2, self->rate / f_m / 4.0);
#endif
		self->max_f[i] = 0;
		bandpass_setup(&self->flt_l[i], self->rate, f_m, bw, 6);
		bandpass_setup(&self->flt_r[i], self->rate, f_m, bw, 6);
		memset(&self->cor[i], 0, sizeof(struct stcorr));

		const float quarterphase = self->rate / f_m / 4.0;

		self->cor[i].yp  = ceil(3.f * quarterphase) - ceil(quarterphase);
		self->cor[i].dls = ceil(3.f * quarterphase);

		self->cor[i].dll = (float*) calloc(self->cor[i].dls, sizeof(float));
		self->cor[i].dlr = (float*) calloc(self->cor[i].dls, sizeof(float));
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
		if (port >= 0 && port < NUM_BANDS) {
			self->spec[port] = (float*) data;
		}
		if (port >= NUM_BANDS && port < NUM_BANDS*2) {
			self->maxf[port-NUM_BANDS] = (float*) data;
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
	float max_f[NUM_BANDS];
	const float omega = self->omega;
	const float gain  = self->gain_coeff;
	struct FilterBank *flt_l[NUM_BANDS];
	struct FilterBank *flt_r[NUM_BANDS];
	struct stcorr     *p_cor[NUM_BANDS];

	for(int i=0; i < NUM_BANDS; ++i) {
		max_f[i] = self->max_f[i];
		flt_l[i] = &self->flt_l[i];
		flt_r[i] = &self->flt_r[i];
		p_cor[i] = &self->cor[i];
	}

	/* calculate total phase */
	self->stcor->process(self->input[0], self->input[1] , n_samples);
	*self->p_phase = self->stcor->read();

	/* per frequency-band phase & amplitude */
	for (uint32_t j = 0 ; j < n_samples; ++j) {
		const float L = *(inL++) * gain;
		const float R = *(inR++) * gain;

		for(int i = 0; i < NUM_BANDS; ++i) {
			const float fl = bandpass_process(flt_l[i], L);
			const float fr = bandpass_process(flt_r[i], R);

			stc_proc_one(p_cor[i], omega, fl, fr);

			const float v = (fl * fl + fr * fr) / 2.f;
			max_f[i] += omega * (v - max_f[i]);
		}
	}

	/* copy back variables and assign value */
	for(int i=0; i < NUM_BANDS; ++i) {
		stc_proc_end(&self->cor[i]);

		for (uint32_t j=0; j < flt_l[i]->filter_stages; ++j) {
			if (!finite(flt_l[i]->f[j].z[0])) flt_l[i]->f[j].z[0] = 0;
			if (!finite(flt_l[i]->f[j].z[1])) flt_l[i]->f[j].z[1] = 0;
			if (!finite(flt_r[i]->f[j].z[0])) flt_r[i]->f[j].z[0] = 0;
			if (!finite(flt_r[i]->f[j].z[1])) flt_r[i]->f[j].z[1] = 0;
		}

		if (!finite(max_f[i])) max_f[i] = 0;
		const float pc = stc_read(&self->cor[i]);

		if (pc != -100) {
			const float mx = sqrtf(2.f * max_f[i]);
			*(self->maxf[i]) = mx > .001f ? (mx > 1.0 ? 0 : 20.0 * log10f(mx)) : -60.0;
			*(self->spec[i]) = pc;
		} else {
			*(self->maxf[i]) = -60;
			*(self->spec[i]) = 0;
		}

		/* copy back locally cached data */
		self->max_f[i] = max_f[i];
		max_f[i] += 1e-10;
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
	for (uint32_t i=0; i < NUM_BANDS; ++i) {
		free(self->cor[i].dll);
		free(self->cor[i].dlr);
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
