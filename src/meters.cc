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

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "../jmeters/jmeterdsp.h"
#include "../jmeters/vumeterdsp.h"
#include "../jmeters/iec1ppmdsp.h"
#include "../jmeters/iec2ppmdsp.h"
#include "../jmeters/stcorrdsp.h"
#include "../jmeters/truepeakdsp.h"
#include "../jmeters/kmeterdsp.h"
#include "../ebumeter/ebu_r128_proc.h"

#include "uris.h"

using namespace LV2M;

typedef enum {
	MTR_REFLEVEL = 0,
	MTR_INPUT0   = 1,
	MTR_OUTPUT0  = 2,
	MTR_LEVEL0   = 3,
	MTR_INPUT1   = 4,
	MTR_OUTPUT1  = 5,
	MTR_LEVEL1   = 6,
	MTR_PEAK0    = 7,
	MTR_PEAK1    = 8,
	MTR_HOLD     = 9
} PortIndex;

typedef struct {
	float  rlgain;
	float  p_refl;
	float* reflvl;

	JmeterDSP *mtr[2];
	Stcorrdsp *cor;
	Ebu_r128_proc *ebu;

	float* level[2];
	float* input[2];
	float* output[2];
	float* peak[2];
	float* hold;

	int chn;
	float peak_max[2];
	float peak_hold;

	/* ebur specific */
  LV2_URID_Map* map;
  EBULV2URIs uris;

  LV2_Atom_Forge forge;
  LV2_Atom_Forge_Frame frame;
  const LV2_Atom_Sequence* control;
  LV2_Atom_Sequence* notify;

	double rate;
	bool ui_active;
	int follow_transport_mode; // bit1: follow start/stop, bit2: reset on re-start.

	bool tranport_rolling;
	bool ebu_integrating;
	bool dbtp_enable;

	float *radarS, radarSC;
	float *radarM, radarMC;
	int radar_pos_cur, radar_pos_max;
	uint32_t radar_spd_cur, radar_spd_max;
	int radar_resync;
	uint64_t integration_time;
	bool send_state_to_ui;
	uint32_t ui_settings;
	float tp_max;

	int histM[HIST_LEN];
	int histS[HIST_LEN];
	int hist_maxM;
	int hist_maxS;

} LV2meter;


#define MTRDEF(NAME, CLASS) \
	else if (!strcmp(descriptor->URI, MTR_URI NAME "mono") || !strcmp(descriptor->URI, MTR_URI NAME "mono_gtk")) { \
		self->chn = 1; \
		self->mtr[0] = new CLASS(); \
		static_cast<CLASS *>(self->mtr[0])->init(rate); \
	} \
	else if (!strcmp(descriptor->URI, MTR_URI NAME "stereo") || !strcmp(descriptor->URI, MTR_URI NAME "stereo_gtk")) { \
		self->chn = 2; \
		self->mtr[0] = new CLASS(); \
		self->mtr[1] = new CLASS(); \
		static_cast<CLASS *>(self->mtr[0])->init(rate); \
		static_cast<CLASS *>(self->mtr[1])->init(rate); \
	}

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	LV2meter* self = (LV2meter*)calloc(1, sizeof(LV2meter));

	if (!self) return NULL;

	if (!strcmp(descriptor->URI, MTR_URI "COR") || !strcmp(descriptor->URI, MTR_URI "COR_gtk")) {
		self->cor = new Stcorrdsp();
		self->cor->init(rate, 2e3f, 0.3f);
	}
	MTRDEF("VU", Vumeterdsp)
	MTRDEF("BBC", Iec2ppmdsp)
	MTRDEF("EBU", Iec2ppmdsp)
	MTRDEF("DIN", Iec1ppmdsp)
	MTRDEF("NOR", Iec1ppmdsp)
	MTRDEF("dBTP", TruePeakdsp)
	MTRDEF("K12", Kmeterdsp)
	MTRDEF("K14", Kmeterdsp)
	MTRDEF("K20", Kmeterdsp)
	else {
		free(self);
		return NULL;
	}

	self->rlgain = 1.0;
	self->p_refl = -9999;

	self->peak_max[0] = 0;
	self->peak_max[1] = 0;
	self->peak_hold   = 0;

	return (LV2_Handle)self;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	LV2meter* self = (LV2meter*)instance;

	switch ((PortIndex)port) {
	case MTR_REFLEVEL:
		self->reflvl = (float*) data;
		break;
	case MTR_INPUT0:
		self->input[0] = (float*) data;
		break;
	case MTR_OUTPUT0:
		self->output[0] = (float*) data;
		break;
	case MTR_LEVEL0:
		self->level[0] = (float*) data;
		break;
	case MTR_INPUT1:
		self->input[1] = (float*) data;
		break;
	case MTR_OUTPUT1:
		self->output[1] = (float*) data;
		break;
	case MTR_LEVEL1:
		self->level[1] = (float*) data;
		break;
	case MTR_PEAK0:
		self->peak[0] = (float*) data;
		break;
	case MTR_PEAK1:
		self->peak[1] = (float*) data;
		break;
	case MTR_HOLD:
		self->hold = (float*) data;
		break;
	}
}


static void
run(LV2_Handle instance, uint32_t n_samples)
{
	LV2meter* self = (LV2meter*)instance;

	if (self->p_refl != *self->reflvl) {
		self->p_refl = *self->reflvl;
		self->rlgain = powf (10.0f, 0.05f * (self->p_refl + 18.0));
	}

	int c;
	for (c = 0; c < self->chn; ++c) {

		float* const input  = self->input[c];
		float* const output = self->output[c];

		self->mtr[c]->process(input, n_samples);

		*self->level[c] = self->rlgain * self->mtr[c]->read();

		if (input != output) {
			memcpy(output, input, sizeof(float) * n_samples);
		}
	}
}


static void
kmeter_run(LV2_Handle instance, uint32_t n_samples)
{
	LV2meter* self = (LV2meter*)instance;
	bool reinit_gui = false;

	/* re-use port 0 to request/notify UI about
	 * peak values - force change to ports */
	if (self->p_refl != *self->reflvl) {
		if (*self->reflvl >= 0) {
			self->peak_hold = 0;
			reinit_gui = true;
		}

		if (*self->reflvl == -1) {
			reinit_gui = true;
		} else {
			self->p_refl = *self->reflvl;
		}
	}

	int c;
	for (c = 0; c < self->chn; ++c) {

		float* const input  = self->input[c];
		float* const output = self->output[c];

		self->mtr[c]->process(input, n_samples);

		if (input != output) {
			memcpy(output, input, sizeof(float) * n_samples);
		}
	}

	if (reinit_gui) {
		/* force parameter change */
		if (self->chn == 1) {
			*self->output[0] = -1 - (rand() & 0xffff); // portindex 5
		} else if (self->chn == 2) {
			*self->hold = -1 - (rand() & 0xffff);
		}
		return;
	}

	if (self->chn == 1) {
		float m, p;
		static_cast<Kmeterdsp*>(self->mtr[0])->read(m, p);
		*self->level[0] = self->rlgain * m;
		*self->input[1] = self->rlgain * p; // portindex 4
		if (*self->input[1] > self->peak_hold) self->peak_hold = *self->input[1];
		*self->output[1] = self->peak_hold; // portindex 5
	} else if (self->chn == 2) {
		float m, p;
		static_cast<Kmeterdsp*>(self->mtr[0])->read(m, p);
		*self->level[0] = self->rlgain * m;
		*self->peak[0] = self->rlgain * p;
		if (*self->peak[0] > self->peak_hold) self->peak_hold = *self->peak[0];

		static_cast<Kmeterdsp*>(self->mtr[1])->read(m, p);
		*self->level[1] = self->rlgain * m;
		*self->peak[1] = self->rlgain * p;
		if (*self->peak[1] > self->peak_hold) self->peak_hold = *self->peak[1];

		*self->hold = self->peak_hold;
	}
}


static void
cleanup(LV2_Handle instance)
{
	LV2meter* self = (LV2meter*)instance;
	for (int c = 0; c < self->chn; ++c) {
		delete self->mtr[c];
	}
	free(instance);
}

static void
dbtp_run(LV2_Handle instance, uint32_t n_samples)
{
	LV2meter* self = (LV2meter*)instance;
	bool reinit_gui = false;

	/* re-use port 0 to request/notify UI about
	 * peak values - force change to ports */
	if (self->p_refl != *self->reflvl) {
		if (*self->reflvl >= 0) {
			self->peak_max[0] = 0;
			self->peak_max[1] = 0;
		}
		if (*self->reflvl == -1) {
			reinit_gui = true;
		} else {
			self->p_refl = *self->reflvl;
		}
	}

	int c;
	for (c = 0; c < self->chn; ++c) {

		float* const input  = self->input[c];
		float* const output = self->output[c];

		self->mtr[c]->process(input, n_samples);

		if (input != output) {
			memcpy(output, input, sizeof(float) * n_samples);
		}
	}

	if (reinit_gui) {
		/* force parameter change */
		if (self->chn == 1) {
			*self->level[0] = -1 - (rand() & 0xffff);
			*self->input[1] = -1; // portindex 4
		} else if (self->chn == 2) {
			*self->level[0] = -1 - (rand() & 0xffff);
			*self->level[1] = -1;
			*self->peak[0] = -1;
			*self->peak[1] = -1;
		}
		return;
	}

	if (self->chn == 1) {
		float m, p;
		static_cast<TruePeakdsp*>(self->mtr[0])->read(m, p);
		if (self->peak_max[0] < self->rlgain * p) { self->peak_max[0] = self->rlgain * p; }
		*self->level[0] = self->rlgain * m;
		*self->input[1] = self->peak_max[0]; // portindex 4
	} else if (self->chn == 2) {
		float m, p;
		static_cast<TruePeakdsp*>(self->mtr[0])->read(m, p);
		if (self->peak_max[0] < self->rlgain * p) { self->peak_max[0] = self->rlgain * p; }
		*self->level[0] = self->rlgain * m;
		*self->peak[0] = self->peak_max[0];
		static_cast<TruePeakdsp*>(self->mtr[1])->read(m, p);
		if (self->peak_max[1] < self->rlgain * p) { self->peak_max[1] = self->rlgain * p; }
		*self->level[1] = self->rlgain * m;
		*self->peak[1] = self->peak_max[1];
	}
}


static void
cor_run(LV2_Handle instance, uint32_t n_samples)
{
	LV2meter* self = (LV2meter*)instance;

	self->cor->process(self->input[0], self->input[1] , n_samples);
	*self->level[0] = self->cor->read();

	if (self->input[0] != self->output[0]) {
		memcpy(self->output[0], self->input[0], sizeof(float) * n_samples);
	}
	if (self->input[1] != self->output[1]) {
		memcpy(self->output[1], self->input[1], sizeof(float) * n_samples);
	}
}

static void
cor_cleanup(LV2_Handle instance)
{
	LV2meter* self = (LV2meter*)instance;
	delete self->cor;
	free(instance);
}


const void*
extension_data(const char* uri)
{
	return NULL;
}

#include "ebulv2.cc"
#include "goniometerlv2.c"
#include "spectrumlv2.c"

#define mkdesc(ID, NAME, RUN) \
static const LV2_Descriptor descriptor ## ID = { \
	MTR_URI NAME, \
	instantiate, \
	connect_port, \
	NULL, \
	RUN, \
	NULL, \
	cleanup, \
	extension_data \
};

mkdesc(0, "VUmono",   run)
mkdesc(1, "VUstereo", run)
mkdesc(2, "BBCmono",  run)
mkdesc(3, "BBCstereo",run)
mkdesc(4, "EBUmono",  run)
mkdesc(5, "EBUstereo",run)
mkdesc(6, "DINmono",  run)
mkdesc(7, "DINstereo",run)
mkdesc(8, "NORmono",  run)
mkdesc(9, "NORstereo",run)

mkdesc(14,"dBTPmono",   dbtp_run)
mkdesc(15,"dBTPstereo", dbtp_run)

mkdesc(16, "VUmono_gtk",   run)
mkdesc(17, "VUstereo_gtk", run)
mkdesc(18, "BBCmono_gtk",  run)
mkdesc(19, "BBCstereo_gtk",run)
mkdesc(20, "EBUmono_gtk",  run)
mkdesc(21, "EBUstereo_gtk",run)
mkdesc(22, "DINmono_gtk",  run)
mkdesc(23, "DINstereo_gtk",run)
mkdesc(24, "NORmono_gtk",  run)
mkdesc(25, "NORstereo_gtk",run)

mkdesc(29,"dBTPmono_gtk",   dbtp_run)
mkdesc(30,"dBTPstereo_gtk", dbtp_run)

mkdesc(32,"K12mono", kmeter_run)
mkdesc(33,"K14mono", kmeter_run)
mkdesc(34,"K20mono", kmeter_run)
mkdesc(35,"K12stereo", kmeter_run)
mkdesc(36,"K14stereo", kmeter_run)
mkdesc(37,"K20stereo", kmeter_run)

mkdesc(38,"K12mono_gtk", kmeter_run)
mkdesc(39,"K14mono_gtk", kmeter_run)
mkdesc(40,"K20mono_gtk", kmeter_run)
mkdesc(41,"K12stereo_gtk", kmeter_run)
mkdesc(42,"K14stereo_gtk", kmeter_run)
mkdesc(43,"K20stereo_gtk", kmeter_run)

static const LV2_Descriptor descriptorCor = {
	MTR_URI "COR",
	instantiate,
	connect_port,
	NULL,
	cor_run,
	NULL,
	cor_cleanup,
	extension_data
};

static const LV2_Descriptor descriptorCorGtk = {
	MTR_URI "COR_gtk",
	instantiate,
	connect_port,
	NULL,
	cor_run,
	NULL,
	cor_cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case  0: return &descriptor0;
	case  1: return &descriptor1;
	case  2: return &descriptor2;
	case  3: return &descriptor3;
	case  4: return &descriptor4;
	case  5: return &descriptor5;
	case  6: return &descriptor6;
	case  7: return &descriptor7;
	case  8: return &descriptor8;
	case  9: return &descriptor9;
	case 10: return &descriptorCor;
	case 11: return &descriptorEBUr128;
	case 12: return &descriptorGoniometer;
	case 13: return &descriptorSpectrum1;
	case 14: return &descriptor14;
	case 15: return &descriptor15;
	case 16: return &descriptor16;
	case 17: return &descriptor17;
	case 18: return &descriptor18;
	case 19: return &descriptor19;
	case 20: return &descriptor20;
	case 21: return &descriptor21;
	case 22: return &descriptor22;
	case 23: return &descriptor23;
	case 24: return &descriptor24;
	case 25: return &descriptor25;
	case 26: return &descriptorGoniometerGtk;
	case 27: return &descriptorSpectrum1Gtk;
	case 28: return &descriptorCorGtk;
	case 29: return &descriptor29;
	case 30: return &descriptor30;
	case 31: return &descriptorEBUr128Gtk;
	case 32: return &descriptor32;
	case 33: return &descriptor33;
	case 34: return &descriptor34;
	case 35: return &descriptor35;
	case 36: return &descriptor36;
	case 37: return &descriptor37;
	case 38: return &descriptor38;
	case 39: return &descriptor39;
	case 40: return &descriptor40;
	case 41: return &descriptor41;
	case 42: return &descriptor42;
	case 43: return &descriptor43;
	case 44: return &descriptorSpectrum2;
	case 45: return &descriptorSpectrum2Gtk;
	default: return NULL;
	}
}
