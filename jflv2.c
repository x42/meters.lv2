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

typedef enum {
	JF_INPUT0   = 0,
	JF_OUTPUT0  = 1,
	JF_INPUT1   = 2,
	JF_OUTPUT1  = 3,
	JF_NOTIFY   = 4,
} JFPortIndex;

#include "jf.h"

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
	if (strcmp(descriptor->URI, MTR_URI "jf")) {
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

	LV2jf* self = (LV2jf*)calloc(1, sizeof(LV2jf));
	if (!self) return NULL;

	self->rate = rate;
	self->ui_active = false;

	self->apv = rint(rate / 15.0);
	self->sample_cnt = 0;
	self->ntfy = 0;

	uint32_t rbsize = self->rate / 4;
	if (rbsize < 8192u) rbsize = 8192u;
	if (rbsize < 2 * self->apv) rbsize = 2 * self->apv;

	self->rb = jfrb_alloc(rbsize);

	return (LV2_Handle)self;
}

static void
goniometer_connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	LV2jf* self = (LV2jf*)instance;
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
	case JF_NOTIFY:
		self->notify = (float*) data;
		break;
	}
}

static void
goniometer_run(LV2_Handle instance, uint32_t n_samples)
{
	LV2jf* self = (LV2jf*)instance;

	if (self->ui_active) {
		jfrb_write(self->rb, self->input[0], self->input[1], n_samples);

		/* notify UI by creating a port-event */
		self->sample_cnt += n_samples;
		if (self->sample_cnt >= self->apv) {
			self->ntfy = (self->ntfy + 1) % 10000;
			self->sample_cnt = self->sample_cnt % self->apv;
		}
		*self->notify = self->ntfy;
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
	LV2jf* self = (LV2jf*)instance;
	jfrb_free(self->rb);
	free(instance);
}

static const LV2_Descriptor descriptorGoniometer = {
	MTR_URI "jf",
	goniometer_instantiate,
	goniometer_connect_port,
	NULL,
	goniometer_run,
	NULL,
	goniometer_cleanup,
	extension_data
};

