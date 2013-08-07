/* ebu-r128 LV2 GUI
 *
 * Copyright 2011-2012 David Robillard <d@drobilla.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#include "./uris.h"

typedef struct {
	LV2_Atom_Forge forge;

	LV2_URID_Map* map;
	EBULV2URIs   uris;

	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	GtkWidget* box;
	GtkWidget* label;

	GtkWidget* btn_box;
	GtkWidget* btn_start;
	GtkWidget* btn_reset;

	GtkWidget* cbx_box;
	GtkWidget* cbx_lufs;
	GtkWidget* cbx_transport;
	GtkWidget* cbx_autoreset;
	bool disable_signals;

	// current data
	float lm, mm, ls, ms, il, rn, rx;

} EBUrUI;

/******************************************************************************
 * LV2 UI -> plugin communication
 */

static void forge_message_kv(EBUrUI* ui, LV2_URID uri, int key, float value) {
  uint8_t obj_buf[1024];
	if (ui->disable_signals) return;

  lv2_atom_forge_set_buffer(&ui->forge, obj_buf, 1024);
	LV2_Atom* msg = forge_kvcontrolmessage(&ui->forge, &ui->uris, uri, key, value);
  ui->write(ui->controller, 0, lv2_atom_total_size(msg), ui->uris.atom_eventTransfer, msg);
}

/******************************************************************************
 * UI callbacks
 */

static gboolean btn_start(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->btn_start))) {
		gtk_button_set_label(GTK_BUTTON(ui->btn_start), "pause");
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_START, 0);
	} else {
		gtk_button_set_label(GTK_BUTTON(ui->btn_start), "start");
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_PAUSE, 0);
	}
	return TRUE;
}

static gboolean btn_reset(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
  forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_RESET, 0);
	return TRUE;
}

static gboolean cbx_transport(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_transport))) {
		gtk_widget_set_sensitive(ui->btn_start, false);
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_TRANSPORTSYNC, 1);
	} else {
		gtk_widget_set_sensitive(ui->btn_start, true);
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_AUTORESET, 0);
	}
	return TRUE;
}

static gboolean cbx_autoreset(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_autoreset))) {
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, 5, 1);
	} else {
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, 5, 0);
	}
	return TRUE;
}

static gboolean cbx_lufs(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	gtk_label_set_text(GTK_LABEL(ui->label), "?");
	return TRUE;
}


/******************************************************************************
 * LV2 callbacks
 */

static LV2UI_Handle
instantiate(const LV2UI_Descriptor*   descriptor,
            const char*               plugin_uri,
            const char*               bundle_path,
            LV2UI_Write_Function      write_function,
            LV2UI_Controller          controller,
            LV2UI_Widget*             widget,
            const LV2_Feature* const* features)
{
	EBUrUI* ui = (EBUrUI*)calloc(1,sizeof(EBUrUI));
	ui->write      = write_function;
	ui->controller = controller;

	*widget = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID_URI "#map")) {
			ui->map = (LV2_URID_Map*)features[i]->data;
		}
	}

	if (!ui->map) {
		fprintf(stderr, "UI: Host does not support urid:map\n");
		free(ui);
		return NULL;
	}

	map_eburlv2_uris(ui->map, &ui->uris);

	lv2_atom_forge_init(&ui->forge, ui->map);

	ui->box = gtk_vbox_new(FALSE, 2);

	ui->label = gtk_label_new("?");

	ui->btn_box = gtk_hbox_new(TRUE, 0);
	ui->btn_start = gtk_toggle_button_new_with_label("start");
	ui->btn_reset = gtk_button_new_with_label("reset");

	ui->cbx_box = gtk_vbox_new(TRUE, 0);
	ui->cbx_lufs      = gtk_check_button_new_with_label("display LUFS");
	ui->cbx_transport = gtk_check_button_new_with_label("use host's transport");
	ui->cbx_autoreset = gtk_check_button_new_with_label("reset when starting");

	//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->btn_pause), TRUE);

	gtk_box_pack_start(GTK_BOX(ui->btn_box), ui->btn_start, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(ui->btn_box), ui->btn_reset, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(ui->cbx_box), ui->cbx_lufs     , FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ui->cbx_box), ui->cbx_transport, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ui->cbx_box), ui->cbx_autoreset, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(ui->box), ui->label, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->cbx_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->btn_box, TRUE, TRUE, 2);

	g_signal_connect (G_OBJECT (ui->btn_start), "toggled", G_CALLBACK (btn_start), ui);
	g_signal_connect (G_OBJECT (ui->btn_reset), "clicked", G_CALLBACK (btn_reset), ui);
	g_signal_connect (G_OBJECT (ui->cbx_lufs),  "toggled", G_CALLBACK (cbx_lufs), ui);
	g_signal_connect (G_OBJECT (ui->cbx_transport), "toggled", G_CALLBACK (cbx_transport), ui);
	g_signal_connect (G_OBJECT (ui->cbx_autoreset), "toggled", G_CALLBACK (cbx_autoreset), ui);

	gtk_widget_show_all(ui->box);
	*widget = ui->box;

  forge_message_kv(ui, ui->uris.mtr_meters_on, 0, 0);
	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	EBUrUI* ui = (EBUrUI*)handle;
  forge_message_kv(ui, ui->uris.mtr_meters_off, 0, 0);
	// TODO clean up gtk widgets..
	free(ui);
}

#define PARSE_A_FLOAT(var) \
	if (var && var->type == uris->atom_Float) { \
		ui->var = ((LV2_Atom_Float*)var)->body; \
	}

#define LUFS(V) ((V) < -100 ? -INFINITY : (lufs ? (V) : (V) + 23.0))

static void update_display(EBUrUI* ui) {
	// TODO queue gtk redraw only
	bool lufs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_lufs));
	char buf[1024];
	sprintf(buf,
			"EBU R128 - GUI work in progress\n\n"
			"---Mid---\nLVL: %.3f\nMAX: %.3f\n"
			"---Slow---\nLVL: %.3f\nMAX: %.3f\n"
			"---Integ---\nLVL: %.3f\nRange: %.3f..%.3f\nRange: %.3f\n",
			LUFS(ui->lm), LUFS(ui->mm),
			LUFS(ui->ls), LUFS(ui->ms),
			LUFS(ui->il),
			LUFS(ui->rn), LUFS(ui->rx), (ui->rx - ui->rn)
			);
	gtk_label_set_text(GTK_LABEL(ui->label), buf);
}

static void parse_ebulevels(EBUrUI* ui, const LV2_Atom_Object* obj) {
	const EBULV2URIs* uris = &ui->uris;
	LV2_Atom *lm = NULL;
	LV2_Atom *mm = NULL;
	LV2_Atom *ls = NULL;
	LV2_Atom *ms = NULL;
	LV2_Atom *il = NULL;
	LV2_Atom *rn = NULL;
	LV2_Atom *rx = NULL;
	LV2_Atom *ii = NULL;

	lv2_atom_object_get(obj,
			uris->ebu_loudnessM, &lm,
			uris->ebu_maxloudnM, &mm,
			uris->ebu_loudnessS, &ls,
			uris->ebu_maxloudnS, &ms,
			uris->ebu_integrated, &il,
			uris->ebu_range_min, &rn,
			uris->ebu_range_max, &rx,
			uris->ebu_integrating, &ii,
			NULL
			);

	PARSE_A_FLOAT(lm)
	PARSE_A_FLOAT(mm)
	PARSE_A_FLOAT(ls)
	PARSE_A_FLOAT(ms)
	PARSE_A_FLOAT(il)
	PARSE_A_FLOAT(rn)
	PARSE_A_FLOAT(rx)

	if (ii && ii->type == uris->atom_Bool) {
		bool ix = ((LV2_Atom_Bool*)ii)->body;
	  bool bx = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->btn_start));
		if (ix != bx) {
			ui->disable_signals = true;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->btn_start), ix);
			ui->disable_signals = false;
		}
	}
}

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	EBUrUI* ui = (EBUrUI*)handle;
	const EBULV2URIs* uris = &ui->uris;

	if (format == uris->atom_eventTransfer) {
		LV2_Atom* atom = (LV2_Atom*)buffer;

		if (atom->type == uris->atom_Blank) {
			LV2_Atom_Object* obj = (LV2_Atom_Object*)atom;
			if (obj->body.otype == uris->mtr_ebulevels) {
				parse_ebulevels(ui, obj);
				update_display(ui);
			} else if (obj->body.otype == uris->mtr_control) {
				int k; float v;
				get_cc_key_value(&ui->uris, obj, &k, &v);
				if (k == CTL_LV2_FTM) {
					int vv = v;
					ui->disable_signals = true;
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cbx_autoreset), (vv&2)==2);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cbx_transport), (vv&1)==1);
					ui->disable_signals = false;
				}
			} else if (obj->body.otype == uris->rdr_radarpoint) {
				// TODO..
			} else {
				fprintf(stderr, "UI: Unknown control message.\n");
			}
		} else {
			fprintf(stderr, "UI: Unknown message type.\n");
		}
	}
}

/******************************************************************************
 * LV2 setup
 */
static const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2UI_Descriptor descriptor = {
	"http://gareus.org/oss/lv2/meters#eburui",
	instantiate,
	cleanup,
	port_event,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor*
lv2ui_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
