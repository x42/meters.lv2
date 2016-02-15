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
	printf("%s\n", descriptor->URI);

	if (   !strcmp (descriptor->URI, MTR_URI "surround8")
			|| !strcmp (descriptor->URI, MTR_URI "surround8_gtk")
		 )
	{
		self->chn = 8;
		self->mtr = (JmeterDSP **) malloc (self->chn * sizeof (JmeterDSP *));
		for (uint32_t i = 0; i < self->chn; ++i) {
			self->mtr[i] = new Kmeterdsp();
			static_cast<Kmeterdsp *>(self->mtr[i])->init(rate);
		}
	} else {
		free(self);
		return NULL;
	}

	self->level = (float**) calloc (self->chn, sizeof (float*));
	self->input = (float**) calloc (self->chn, sizeof (float*));
	self->output = (float**) calloc (self->chn, sizeof (float*));

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
	else if (port > 0 && port <= 3 * self->chn) {
		int chan = (port - 1) / 3;
		switch (port % 3) {
			case 1:
				self->input[chan] = (float*) data;
				break;
			case 2:
				self->output[chan] = (float*) data;
				break;
			case 0:
				self->level[chan] = (float*) data;
				break;
		}
	}
}

static const void*
extension_data_sur(const char* uri)
{
  return NULL;
}

static const LV2_Descriptor descriptorSUR8 = {
	MTR_URI "surround8",
	sur_instantiate,
	sur_connect_port,
	NULL,
	run,
	NULL,
	cleanup,
	extension_data_sur
};

static const LV2_Descriptor descriptorSUR8Gtk = {
	MTR_URI "surround8_gtk",
	sur_instantiate,
	sur_connect_port,
	NULL,
	run,
	NULL,
	cleanup,
	extension_data_sur
};
