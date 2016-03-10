/* meter.lv2 -- surround meter
 *
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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
 * LV2
 */

static LV2_Handle
sur_instantiate(
		const LV2_Descriptor*     descriptor,
		double                    rate,
		const char*               bundle_path,
		const LV2_Feature* const* features)
{
	LV2meter* self = (LV2meter*)calloc (1, sizeof (LV2meter));
	if (!self) return NULL;

	if (       !strcmp (descriptor->URI, MTR_URI "surround8")) {
		self->chn = 8;
	} else if (!strcmp (descriptor->URI, MTR_URI "surround5")) {
		self->chn = 5;
	} else {
		free(self);
		return NULL;
	}

	self->mtr = (JmeterDSP **) malloc (self->chn * sizeof (JmeterDSP *));
	for (uint32_t i = 0; i < self->chn; ++i) {
		self->mtr[i] = new Kmeterdsp();
		static_cast<Kmeterdsp *>(self->mtr[i])->init(rate);
	}

	self->level  = (float**) calloc (self->chn, sizeof (float*));
	self->input  = (float**) calloc (self->chn, sizeof (float*));
	self->output = (float**) calloc (self->chn, sizeof (float*));
	self->peak   = (float**) calloc (self->chn, sizeof (float*));

	for (uint32_t c = 0; c < 4; ++c) {
		self->cor4[c] = new Stcorrdsp();
		self->cor4[c]->init (rate, 2e3f, 0.3f);
	}
	self->rlgain = 1.0;
	self->p_refl = -9999;

	return (LV2_Handle)self;
}


static void
sur_connect_port (LV2_Handle instance, uint32_t port, void* data)
{
	LV2meter* self = (LV2meter*)instance;
	if (port == MTR_REFLEVEL) {
		self->reflvl = (float*) data;
	}
	else if (port >  0 && port <= 12) {
		// correlation
		int cor = (port - 1) / 3;
		switch (port % 3) {
			case 1:
				self->surc_a[cor] = (float*) data;
				break;
			case 2:
				self->surc_b[cor] = (float*) data;
				break;
			case 0:
				self->surc_c[cor] = (float*) data;
				break;
		}
	}
	else if (port > 12 && port <= 12 + 4 * self->chn) {
		int chan = (port - 13) / 4;
		switch (port % 4) {
			case 1:
				self->input[chan] = (float*) data;
				break;
			case 2:
				self->output[chan] = (float*) data;
				break;
			case 3:
				self->level[chan] = (float*) data;
				break;
			case 0:
				self->peak[chan] = (float*) data;
				break;
		}
	}
}

static void
sur_run(LV2_Handle instance, uint32_t n_samples)
{
	LV2meter* self = (LV2meter*)instance;

	if (self->p_refl != *self->reflvl) {
		self->p_refl = *self->reflvl;
		self->rlgain = powf (10.0f, 0.05f * (self->p_refl + 18.0));
	}

	for (uint32_t c = 0; c < 4; ++c) {
		uint32_t in_a = rintf (*self->surc_a[c]);
		uint32_t in_b = rintf (*self->surc_b[c]);
		if (in_a < 0) in_a = 0;
		if (in_b < 0) in_b = 0;
		if (in_a >= self->chn) in_a = self->chn - 1;
		if (in_b >= self->chn) in_b = self->chn - 1;
		self->cor4[c]->process (self->input[in_a], self->input[in_b], n_samples);
		*self->surc_c[c] = self->cor4[c]->read();
	}

	for (uint32_t c = 0; c < self->chn; ++c) {
		float m, p;

		float* const input  = self->input[c];
		float* const output = self->output[c];

		self->mtr[c]->process(input, n_samples);

		static_cast<Kmeterdsp*>(self->mtr[c])->read(m, p);

		*self->level[c] = self->rlgain * m;
		*self->peak[c]  = self->rlgain * p;

		if (input != output) {
			memcpy(output, input, sizeof(float) * n_samples);
		}
	}
}
static void
sur_cleanup(LV2_Handle instance)
{
	LV2meter* self = (LV2meter*)instance;
	for (uint32_t c = 0; c < 4; ++c) {
		delete self->cor4[c];
	}
	for (uint32_t c = 0; c < self->chn; ++c) {
		delete self->mtr[c];
	}
	FREE_VARPORTS;
	free (self->mtr);
	free(instance);
}
static const LV2_Descriptor descriptorSUR8 = {
	MTR_URI "surround8",
	sur_instantiate,
	sur_connect_port,
	NULL,
	sur_run,
	NULL,
	sur_cleanup,
	extension_data
};

static const LV2_Descriptor descriptorSUR5 = {
	MTR_URI "surround5",
	sur_instantiate,
	sur_connect_port,
	NULL,
	sur_run,
	NULL,
	sur_cleanup,
	extension_data
};
