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

#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <pango/pango.h>

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

	GtkWidget* m0;

	bool disable_signals;

	// current data
	float lm, mm, ls, ms, il, rn, rx;

	float *radarS;
	float *radarM;
	int radar_pos_cur;
	int radar_pos_max;

} EBUrUI;

/******************************************************************************
 * custom visuals
 */

#define LUFS(V) ((V) < -100 ? -INFINITY : (lufs ? (V) : (V) + 23.0))

/* colors */
static const float c_red[4] = {1.0, 0.0, 0.0, 1.0};
static const float c_grn[4] = {0.0, 1.0, 0.0, 1.0};
static const float c_blk[4] = {0.0, 0.0, 0.0, 1.0};
static const float c_wht[4] = {1.0, 1.0, 1.0, 1.0};
static const float c_gry[4] = {0.5, 0.5, 0.5, 1.0};

void write_text(PangoContext * pc, cairo_t* cr,
		const char *txt, const char *font,
		const float x, const float y,
		const float ang, const int align,
		const float * const col) {
	int tw, th;
	cairo_save(cr);

	PangoLayout * pl = pango_layout_new (pc);

	if (font) {
		PangoFontDescription *desc = pango_font_description_from_string(font);
		pango_layout_set_font_description(pl, desc);
		pango_font_description_free(desc);
	}
	cairo_set_source_rgba (cr, col[0], col[1], col[2], col[3]);
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, &tw, &th);
	cairo_translate (cr, x, y);
	if (ang != 0) { cairo_rotate (cr, ang); }
	switch(align) {
		case 1:
			cairo_translate (cr, -tw, -th/2.0);
			break;
		case 2:
			cairo_translate (cr, -tw/2.0 - 0.5, -th/2.0);
			break;
		case 3:
			cairo_translate (cr, -0.5, -th/2.0);
			break;
		case 5:
			cairo_translate (cr, -tw/2.0 - 0.5, -th);
			break;
		case 6:
			cairo_translate (cr, -0.5, -th);
			break;
		case 8:
			cairo_translate (cr, -tw/2.0 - 0.5, 0);
			break;
		case 9:
			cairo_translate (cr, -0.5, 0);
			break;
		case 20:
			cairo_translate (cr, -tw, -th);
			break;
		default:
			break;
	}
	pango_cairo_layout_path(cr, pl);
	pango_cairo_show_layout(cr, pl);
	g_object_unref(pl);
	cairo_restore(cr);
	cairo_new_path (cr);
}

void rounded_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
  double degrees = M_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);  //tr
  cairo_arc (cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);  //br
  cairo_arc (cr, x + r, y + h - r, r, 90 * degrees, 180 * degrees);  //bl
  cairo_arc (cr, x + r, y + r, r, 180 * degrees, 270 * degrees);  //tl
  cairo_close_path (cr);
}


static float radar_deflect(const float v, const float r) {
	if (v < -60) return 0;
	if (v > 0) return r;
	return (v + 60.0) * r / 60.0;
}
static void radar_color(cairo_t* cr, const float v, float alpha) {
	if (alpha > 0.9) alpha = 0.9;
	else if (alpha < 0) alpha = 1.0;

	if (v < -70) {
		cairo_set_source_rgba (cr, .3, .3, .3, alpha);
	} else if (v < -45) {
		cairo_set_source_rgba (cr, .0, .0, .5, alpha);
	} else if (v < -35) {
		cairo_set_source_rgba (cr, .0, .0, .9, alpha);
	} else if (v < -23) {
		cairo_set_source_rgba (cr, .0, .6, .0, alpha);
	} else if (v < -15) {
		cairo_set_source_rgba (cr, .0, .9, .0, alpha);
	} else if (v < -12) {
		cairo_set_source_rgba (cr, .75, .75, .0, alpha);
	} else if (v < -6) {
		cairo_set_source_rgba (cr, .8, .4, .0, alpha);
	} else if (v < -3) {
		cairo_set_source_rgba (cr, .8, .0, .0, alpha);
	} else {
		cairo_set_source_rgba (cr, 1.0, .0, .0, alpha);
	}
}

static gboolean expose_event(GtkWidget *w, GdkEventExpose *event, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	bool lufs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_lufs));

	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	PangoContext * pc = gtk_widget_get_pango_context(w);

	char buf[128];
	gint ww, wh;
	gtk_widget_get_size_request(w, &ww, &wh);

	cairo_rectangle (cr, 0, 0, ww, wh);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_fill (cr);

	sprintf(buf, "%+6.2f %s", LUFS(ui->lm), lufs ? "LUFS" : "LU");
	write_text(pc, cr, buf, "Mono 14", ww/2 , 10, 0, 8, c_wht);

	/* radar background */
	const float radius = 120.0;
	const float cx = ww/2.0;
	const float cy = 190;

	cairo_set_source_rgba (cr, .1, .1, .1, 1.0);
	cairo_arc (cr, cx, cy, radius, 0, 2.0 * M_PI);
	cairo_fill (cr);

	cairo_set_line_width(cr, 1.5);
	cairo_arc (cr, cx, cy, radar_deflect(-23, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);

	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgba (cr, .3, .3, .3, 1.0);
	cairo_arc (cr, cx, cy, radar_deflect(-35, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);
	cairo_arc (cr, cx, cy, radar_deflect(-15, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);
	cairo_arc (cr, cx, cy, radar_deflect(-8, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);
	cairo_arc (cr, cx, cy, radar_deflect( 0, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);

	if ( ui->radar_pos_max > 0) {
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

		const double astep = 2.0 * M_PI / (double) ui->radar_pos_max;
		for (int ang = 0; ang < ui->radar_pos_max; ++ang) {
			int age = (ui->radar_pos_max + ang - ui->radar_pos_cur) % ui->radar_pos_max;

			radar_color(cr, ui->radarM[ang], .05 + 3.0 * age / (double) ui->radar_pos_max);

			cairo_move_to(cr, cx, cy);
			cairo_arc (cr, cx, cy, radar_deflect(ui->radarM[ang], radius),
					(double) ang * astep, (ang+1.0) * astep);
			cairo_line_to(cr, cx, cy);
			cairo_fill(cr); // TODO use pattern, fill after loop.
		}

		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		// current position
		cairo_set_source_rgba (cr, .7, .7, .7, 0.3);
		cairo_move_to(cr, cx, cy);
		cairo_arc (cr, cx, cy, radius + 5.0,
					(double) ui->radar_pos_cur * astep, ((double) ui->radar_pos_cur + 1.0) * astep);
		cairo_line_to(cr, cx, cy);
		//cairo_fill_preserve (cr);
		cairo_stroke (cr);

#if 0 // current level
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.3);
		cairo_arc (cr, cx, cy, radar_deflect(ui->lm, radius), 0, 2.0 * M_PI);
		cairo_fill (cr);
#endif

	}

	cairo_set_line_width(cr, 2.5);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_source_rgba (cr, .3, .3, .3, 1.0);
	for (float val = -69; val <= 0; val+=.5) {
		const double ang = val * 2.0 * 3.0 * M_PI / 69.0 / 4.0;
		cairo_save(cr);
		if (val < ui->lm) {
			radar_color(cr, val, -1);
		} else {
			cairo_set_source_rgba (cr, .3, .3, .3, 1.0);
		}
		cairo_translate (cr, cx, cy);
		cairo_rotate (cr, ang);
		cairo_translate (cr, radius + 10.0, 0);
		cairo_move_to(cr,  0.5, 0);
		cairo_line_to(cr,  9.5, 0);
		cairo_stroke (cr);
#if 0
		if ((val+63) %10 == 0) {
			sprintf(buf, "%d", val);
			write_text(pc, cr, buf, "Mono 8", 14, 0, -ang, 3, c_gry);
		}
#endif
		cairo_restore(cr);
	}

	sprintf(buf, "%+.0f", LUFS(-69));
	write_text(pc, cr, buf, "Mono 8", cx, cy + radius + 23, 0, 8, c_gry);

	sprintf(buf, "%+.0f", LUFS(-23));
	write_text(pc, cr, buf, "Mono 8", cx, cy - radius - 23, 0, 5, c_gry);

	sprintf(buf, "%+.0f", LUFS(0));
	write_text(pc, cr, buf, "Mono 8", cx + radius + 23, cy, 0, 3, c_gry);

	sprintf(buf, "%+.0f", LUFS(-46));
	write_text(pc, cr, buf, "Mono 8", cx - radius - 23, cy, 0, 1, c_gry);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->btn_start))) {
		cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
		rounded_rectangle (cr, 5, wh-45, ww-10, 40, 10);
		cairo_fill (cr);

		if (ui->il > -60) {
			sprintf(buf, "Int:   %+6.2f %s", LUFS(ui->lm), lufs ? "LUFS" : "LU");
			write_text(pc, cr, buf, "Mono 9", 15 , wh - 25, 0,  6, c_wht);
		} else {
			sprintf(buf, "[Integrating over 5 sec]");
			write_text(pc, cr, buf, "Sans 9", 15 , wh - 25, 0,  6, c_wht);
		}

		if (ui->rx > -60 && (ui->rx - ui->rn) > 0) {
			sprintf(buf, "Range: %+6.2f..%+6.2f %s (%5.3f)",
					LUFS(ui->rn), LUFS(ui->rx), lufs ? "LUFS" : "LU", (ui->rx - ui->rn));
			write_text(pc, cr, buf, "Mono 9", 15 , wh - 10, 0,  6, c_wht);
		} else {
			sprintf(buf, "[10 sec range.. Please stand by]");
			write_text(pc, cr, buf, "Sans 9", 15 , wh - 10, 0,  6, c_wht);
		}
	}

	cairo_destroy (cr);

	return TRUE;
}


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
	gtk_widget_queue_draw(ui->m0);
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

	ui->m0    = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), 330, 400);
	gtk_widget_set_size_request(ui->m0, 330, 400);

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
	gtk_box_pack_start(GTK_BOX(ui->box), ui->m0, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->cbx_box, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->btn_box, FALSE, FALSE, 2);

	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);
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

#define PARSE_A_FLOAT(var, dest) \
	if (var && var->type == uris->atom_Float) { \
		dest = ((LV2_Atom_Float*)var)->body; \
	}

#define PARSE_A_INT(var, dest) \
	if (var && var->type == uris->atom_Int) { \
		dest = ((LV2_Atom_Int*)var)->body; \
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

	PARSE_A_FLOAT(lm, ui->lm)
	PARSE_A_FLOAT(mm, ui->mm)
	PARSE_A_FLOAT(ls, ui->ls)
	PARSE_A_FLOAT(ms, ui->ms)
	PARSE_A_FLOAT(il, ui->il)
	PARSE_A_FLOAT(rn, ui->rn)
	PARSE_A_FLOAT(rx, ui->rx)

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

static void parse_radarinfo(EBUrUI* ui, const LV2_Atom_Object* obj) {
	const EBULV2URIs* uris = &ui->uris;
	LV2_Atom *lm = NULL;
	LV2_Atom *ls = NULL;
	LV2_Atom *pp = NULL;
	LV2_Atom *pc = NULL;
	LV2_Atom *pm = NULL;

	float xlm, xls;
	int p,c,m;

	xlm = xls = -INFINITY;
	p=c=m=-1;

	lv2_atom_object_get(obj,
			uris->ebu_loudnessM, &lm,
			uris->ebu_loudnessS, &ls,
			uris->rdr_pointpos, &pp,
			uris->rdr_pos_cur, &pc,
			uris->rdr_pos_max, &pm,
			NULL
			);

	PARSE_A_FLOAT(lm, xlm)
	PARSE_A_FLOAT(ls, xls)
	PARSE_A_INT(pp, p);
	PARSE_A_INT(pc, c);
	PARSE_A_INT(pm, m);

	//printf("RADARPOINT %f %f %d %d %d\n", xlm, xls, m, c, p);
	if (m < 1 || c < 0 || p < 0) return;

	if (m != ui->radar_pos_max) {
		//printf("REALLOC %d -> %d\n", ui->radar_pos_max, m);
		ui->radarS = (float*) realloc((void*) ui->radarS, sizeof(float) * m);
		ui->radarM = (float*) realloc((void*) ui->radarM, sizeof(float) * m);
		ui->radar_pos_max = m;
	}
	//printf("Radar: %d -> %f, %f\n", p, xlm, xls);
	ui->radarM[p] = xlm;
	ui->radarS[p] = xls;
	ui->radar_pos_cur = c;
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
				gtk_widget_queue_draw(ui->m0);
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
				parse_radarinfo(ui, obj);
				gtk_widget_queue_draw(ui->m0);
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
