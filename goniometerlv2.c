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
 * broken out goniometer related LV2 functions
 */

#define UPDATE_FPS (30.0)

typedef enum {
	JF_INPUT0   = 0,
	JF_OUTPUT0  = 1,
	JF_INPUT1   = 2,
	JF_OUTPUT1  = 3,
	JF_GAIN     = 4,
	JF_CORR     = 5,
	JF_NOTIFY   = 6,
} JFPortIndex;

#include "goniometer.h"

/******************************************************************************
 * LV2 callbacks
 */

static LV2_Handle
goniometer_instantiate(
		const LV2_Descriptor*     descriptor,
		double                    rate,
		const char*               bundle_path,
		const LV2_Feature* const* features)
{
	if (strcmp(descriptor->URI, MTR_URI "goniometer")) {
		return NULL;
	}

	bool instance_ok = false;
	for (int i=0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, "http://lv2plug.in/ns/ext/instance-access")) {
			instance_ok = true;
		}
	}
	if (!instance_ok) {
		fprintf(stderr, "meters.lv2 error: Host does not support instance-access.\n");
		return NULL;
	}

	LV2gm* self = (LV2gm*)calloc(1, sizeof(LV2gm));
	if (!self) return NULL;

	self->cor = new Stcorrdsp();
	self->cor->init(rate, 2e3f, 0.3f);

	self->rate = rate;
	self->ui_active = false;

	self->apv = rint(rate / UPDATE_FPS);
	self->sample_cnt = 0;
	self->ntfy = 0;

	uint32_t rbsize = self->rate / 4;
	if (rbsize < 8192u) rbsize = 8192u;
	if (rbsize < 2 * self->apv) rbsize = 2 * self->apv;

	self->rb = gmrb_alloc(rbsize);

	return (LV2_Handle)self;
}

static void
goniometer_connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	LV2gm* self = (LV2gm*)instance;
	switch ((JFPortIndex)port) {
	case JF_INPUT0:
		self->input[0] = (float*) data;
		break;
	case JF_OUTPUT0:
		self->output[0] = (float*) data;
		break;
	case JF_INPUT1:
		self->input[1] = (float*) data;
		break;
	case JF_OUTPUT1:
		self->output[1] = (float*) data;
		break;
	case JF_GAIN:
		self->gain = (float*) data;
		break;
	case JF_CORR:
		self->correlation = (float*) data;
		break;
	case JF_NOTIFY:
		self->notify = (float*) data;
		break;
	}
}

static void
goniometer_run(LV2_Handle instance, uint32_t n_samples)
{
	LV2gm* self = (LV2gm*)instance;

	self->cor->process(self->input[0], self->input[1] , n_samples);

	if (self->ui_active) {
		gmrb_write(self->rb, self->input[0], self->input[1], n_samples);

		/* notify UI by creating a port-event */
		self->sample_cnt += n_samples;
		if (self->sample_cnt >= self->apv) {
			self->ntfy = (self->ntfy + 1) % 10000;
			self->sample_cnt = self->sample_cnt % self->apv;
		}
		*self->notify = self->ntfy;
		*self->correlation = self->cor->read();
	}

	if (self->input[0] != self->output[0]) {
		memcpy(self->output[0], self->input[0], sizeof(float) * n_samples);
	}
	if (self->input[1] != self->output[1]) {
		memcpy(self->output[1], self->input[1], sizeof(float) * n_samples);
	}
}

static void
goniometer_cleanup(LV2_Handle instance)
{
	LV2gm* self = (LV2gm*)instance;
	gmrb_free(self->rb);
	delete self->cor;
	free(instance);
}

static const LV2_Descriptor descriptorGoniometer = {
	MTR_URI "goniometer",
	goniometer_instantiate,
	goniometer_connect_port,
	NULL,
	goniometer_run,
	NULL,
	goniometer_cleanup,
	extension_data
};

