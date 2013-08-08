/* ebu-r128 LV2 GUI
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

#ifndef MTR_URIS_H
#define MTR_URIS_H

#include <stdio.h>
#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/time/time.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

#define MTR_URI "http://gareus.org/oss/lv2/meters#"

#define MTR__ebulevels        MTR_URI "ebulevels"
#define MTR_ebu_loudnessM     MTR_URI "ebu_loudnessM"
#define MTR_ebu_maxloudnM     MTR_URI "ebu_maxloudnM"
#define MTR_ebu_loudnessS     MTR_URI "ebu_loudnessS"
#define MTR_ebu_maxloudnS     MTR_URI "ebu_maxloudnS"
#define MTR_ebu_integrated    MTR_URI "ebu_integrated"
#define MTR_ebu_range_min     MTR_URI "ebu_range_min"
#define MTR_ebu_range_max     MTR_URI "ebu_range_max"
#define MTR_ebu_integrating   MTR_URI "ebu_integrating"

#define MTR__rdr_radarpoint   MTR_URI "rdr_radarpoint"
#define MTR__rdr_pointpos     MTR_URI "rdr_pointpos"
#define MTR__rdr_pos_cur      MTR_URI "rdr_pos_cur"
#define MTR__rdr_pos_max      MTR_URI "rdr_pos_max"

#define MTR__cckey    MTR_URI "controlkey"
#define MTR__ccval    MTR_URI "controlval"
#define MTR__control  MTR_URI "control"

#define MTR__meteron  MTR_URI "meteron"
#define MTR__meteroff MTR_URI "meteroff"
#define MTR__metercfg MTR_URI "metercfg"

typedef struct {
	LV2_URID atom_Blank;
	LV2_URID atom_Int;
	LV2_URID atom_Long;
	LV2_URID atom_Float;
	LV2_URID atom_Bool;
	LV2_URID atom_eventTransfer;

	LV2_URID time_Position;
	LV2_URID time_speed;
	LV2_URID time_frame;

	LV2_URID mtr_control; // from backend to UI
	LV2_URID mtr_cckey;
	LV2_URID mtr_ccval;

	LV2_URID mtr_meters_on;
	LV2_URID mtr_meters_off;
	LV2_URID mtr_meters_cfg; // from UI -> backend

	LV2_URID mtr_ebulevels;
	LV2_URID ebu_loudnessM;
	LV2_URID ebu_maxloudnM;
	LV2_URID ebu_loudnessS;
	LV2_URID ebu_maxloudnS;
	LV2_URID ebu_integrated;
	LV2_URID ebu_range_min;
	LV2_URID ebu_range_max;
	LV2_URID ebu_integrating;

	LV2_URID rdr_radarpoint;
	LV2_URID rdr_pointpos;
	LV2_URID rdr_pos_cur;
	LV2_URID rdr_pos_max;

} EBULV2URIs;


// numeric keys
enum {
	KEY_INVALID = 0,
	CTL_START,
	CTL_PAUSE,
	CTL_RESET,
	CTL_TRANSPORTSYNC,
	CTL_AUTORESET,
	CTL_RADARTIME,
	CTL_LV2_RADARTIME,
	CTL_LV2_FTM
};


static inline void
map_eburlv2_uris(LV2_URID_Map* map, EBULV2URIs* uris)
{
	uris->atom_Blank         = map->map(map->handle, LV2_ATOM__Blank);
	uris->atom_Int           = map->map(map->handle, LV2_ATOM__Int);
	uris->atom_Long          = map->map(map->handle, LV2_ATOM__Long);
	uris->atom_Float         = map->map(map->handle, LV2_ATOM__Float);
	uris->atom_Bool          = map->map(map->handle, LV2_ATOM__Bool);

	uris->atom_eventTransfer = map->map(map->handle, LV2_ATOM__eventTransfer);

	uris->time_Position       = map->map(map->handle, LV2_TIME__Position);
	uris->time_speed          = map->map(map->handle, LV2_TIME__speed);
	uris->time_frame          = map->map(map->handle, LV2_TIME__frame);

	uris->mtr_ebulevels       = map->map(map->handle, MTR__ebulevels);
	uris->ebu_loudnessM       = map->map(map->handle, MTR_ebu_loudnessM);
	uris->ebu_maxloudnM       = map->map(map->handle, MTR_ebu_maxloudnM);
	uris->ebu_loudnessS       = map->map(map->handle, MTR_ebu_loudnessS);
	uris->ebu_maxloudnS       = map->map(map->handle, MTR_ebu_maxloudnS);
	uris->ebu_integrated      = map->map(map->handle, MTR_ebu_integrated);
	uris->ebu_range_min       = map->map(map->handle, MTR_ebu_range_min);
	uris->ebu_range_max       = map->map(map->handle, MTR_ebu_range_max);
	uris->ebu_integrating     = map->map(map->handle, MTR_ebu_integrating);

	uris->rdr_radarpoint      = map->map(map->handle, MTR__rdr_radarpoint);
	uris->rdr_pointpos        = map->map(map->handle, MTR__rdr_pointpos);
	uris->rdr_pos_cur         = map->map(map->handle, MTR__rdr_pos_cur);
	uris->rdr_pos_max         = map->map(map->handle, MTR__rdr_pos_max);

	uris->mtr_cckey          = map->map(map->handle, MTR__cckey);
	uris->mtr_ccval          = map->map(map->handle, MTR__ccval);
	uris->mtr_control        = map->map(map->handle, MTR__control);

	uris->mtr_meters_on       = map->map(map->handle, MTR__meteron);
	uris->mtr_meters_off      = map->map(map->handle, MTR__meteroff);
	uris->mtr_meters_cfg      = map->map(map->handle, MTR__metercfg);
}


static inline LV2_Atom *
forge_kvcontrolmessage(LV2_Atom_Forge* forge,
		const EBULV2URIs* uris,
		LV2_URID uri,
		const int key, const float value)
{
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(forge, 0);
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_blank(forge, &frame, 1, uri);

	lv2_atom_forge_property_head(forge, uris->mtr_cckey, 0);
	lv2_atom_forge_int(forge, key);
	lv2_atom_forge_property_head(forge, uris->mtr_ccval, 0);
	lv2_atom_forge_float(forge, value);
	lv2_atom_forge_pop(forge, &frame);
	return msg;
}

static inline int
get_cc_key_value(
		const EBULV2URIs* uris, const LV2_Atom_Object* obj,
		int *k, float *v)
{
	const LV2_Atom* key = NULL;
	const LV2_Atom* value = NULL;
	if (!k || !v) return -1;
	*k = 0; *v = 0.0;

	if (obj->body.otype != uris->mtr_control && obj->body.otype != uris->mtr_meters_cfg) {
		return -1;
	}
	lv2_atom_object_get(obj, uris->mtr_cckey, &key, uris->mtr_ccval, &value, 0);
	if (!key || !value) {
		fprintf(stderr, "MTRlv2: Malformed ctrl message has no key or value.\n");
		return -1;
	}
	*k = ((LV2_Atom_Int*)key)->body;
	*v = ((LV2_Atom_Float*)value)->body;
	return 0;
}

#endif
