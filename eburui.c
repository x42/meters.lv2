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
#define USE_PATTERN

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

	GtkWidget* btn_box;
	GtkWidget* btn_start;
	GtkWidget* btn_reset;

	GtkWidget* cbx_box;
	GtkWidget* cbx_lufs;
	GtkWidget* cbx_slow;
	GtkWidget* cbx_transport;
	GtkWidget* cbx_autoreset;

	GtkWidget* spn_radartime;
	GtkWidget* lbl_radarunit;
	GtkWidget* lbl_radarinfo;

	GtkWidget* m0;
	cairo_pattern_t * cpattern;

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
		case 4:
			cairo_translate (cr, -tw, -th);
			break;
		case 5:
			cairo_translate (cr, -tw/2.0 - 0.5, -th);
			break;
		case 6:
			cairo_translate (cr, -0.5, -th);
			break;
		case 7:
			cairo_translate (cr, -tw, 0);
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
static cairo_pattern_t * radar_pattern(cairo_t* cr, float cx, float cy, float rad) {
	cairo_pattern_t * pat = cairo_pattern_create_radial(cx, cy, 0, cx, cy, rad);
	cairo_pattern_add_color_stop_rgba(pat, 0.0 ,  .0, .0, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, 0.10,  .0, .0, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-45, 1.0),  .0, .0, .5, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-35, 1.0),  .0, .0, .9, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-33, 1.0),  .0, .6, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-23, 1.0),  .0, .9, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-17, 1.0), .75,.75, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-12, 1.0),  .8, .4, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect( -3, 1.0),  .8, .0, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, 1.0 ,  .9, .0, .0, 1.0);

	return pat;
}

static gboolean expose_event(GtkWidget *w, GdkEventExpose *event, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	bool lufs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_lufs));
	bool slow = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_slow));

	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	PangoContext * pc = gtk_widget_get_pango_context(w);

	char buf[128];
	gint ww, wh;
	gtk_widget_get_size_request(w, &ww, &wh);

	const float radius = 120.0;
	const float cx = ww/2.0;
	const float cy = 190;

	/* fill background */
	cairo_rectangle (cr, 0, 0, ww, wh);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_fill (cr);

	write_text(pc, cr, "EBU R128 LV2", "Sans 8", 2 , 5, 1.5 * M_PI, 7, c_gry);

	/* big level as text */
	sprintf(buf, "%+5.1f %s", LUFS( slow? ui->ls : ui->lm), lufs ? "LUFS" : "LU");
	write_text(pc, cr, buf, "Mono 14", ww/2 , 10, 0, 8, c_wht);

	/* max level background */
	int trw = lufs ? 87 : 75;
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	rounded_rectangle (cr, ww-trw-5, 5, trw, 38, 10);
	cairo_fill (cr);

	/* display max level as text */
	sprintf(buf, "Max:\n%+5.1f %s", LUFS( slow ? ui->ms: ui->mm), lufs ? "LUFS" : "LU");
	write_text(pc, cr, buf, "Mono 9", ww-15, 10, 0, 7, c_wht);

#if 1 // radar..
	if (!ui->cpattern) {
		ui->cpattern = radar_pattern(cr, cx, cy, radius);
	}

	/* radar background */
	cairo_set_source_rgba (cr, .05, .05, .05, 1.0);
	cairo_arc (cr, cx, cy, radius, 0, 2.0 * M_PI);
	cairo_fill (cr);

	if ( ui->radar_pos_max > 0) {
		float *rdr = slow ? ui->radarS : ui->radarM;

#ifndef USE_PATTERN
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
#endif
		const double astep = 2.0 * M_PI / (double) ui->radar_pos_max;
		for (int ang = 0; ang < ui->radar_pos_max; ++ang) {
#ifndef USE_PATTERN
			int age = (ui->radar_pos_max + ang - ui->radar_pos_cur) % ui->radar_pos_max;
			radar_color(cr, rdr[ang], .05 + 3.0 * age / (double) ui->radar_pos_max);
#endif

			cairo_move_to(cr, cx, cy);
			cairo_arc (cr, cx, cy, radar_deflect(rdr[ang], radius),
					(double) ang * astep, (ang+1.0) * astep);
			cairo_line_to(cr, cx, cy);
#ifndef USE_PATTERN
			cairo_fill(cr);
#endif
		}
#ifdef USE_PATTERN
		cairo_set_source (cr, ui->cpattern);
		cairo_fill(cr);

		// shade
		for (int p = 0; p < 12; ++p) {
			float pos = ui->radar_pos_cur + 1 + p;
			cairo_set_source_rgba (cr, .0, .0, .0, 1.0 - ((p+1.0)/12.0));
			cairo_move_to(cr, cx, cy);
			cairo_arc (cr, cx, cy, radius,
						pos * astep, (pos + 1.0) * astep);
			cairo_fill(cr);
		}

#else
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
#endif

		// current position
		cairo_set_source_rgba (cr, .7, .7, .7, 0.3);
		cairo_move_to(cr, cx, cy);
		cairo_arc (cr, cx, cy, radius + 5.0,
					(double) ui->radar_pos_cur * astep, ((double) ui->radar_pos_cur + 1.0) * astep);
		cairo_line_to(cr, cx, cy);
		cairo_stroke (cr);
	}

	/* lines */
	cairo_set_source_rgba (cr, .5, .5, .8, 0.75);
	cairo_set_line_width(cr, 1.5);
	cairo_arc (cr, cx, cy, radar_deflect(-23, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);

	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgba (cr, .5, .5, .8, 0.75);
	cairo_arc (cr, cx, cy, radar_deflect(-45, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);
	cairo_arc (cr, cx, cy, radar_deflect(-35, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);
	cairo_arc (cr, cx, cy, radar_deflect(-15, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);
	cairo_arc (cr, cx, cy, radar_deflect(-8, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);
	cairo_arc (cr, cx, cy, radar_deflect( 0, radius), 0, 2.0 * M_PI);
	cairo_stroke (cr);

	float innercircle = radar_deflect(-45, radius);
	for (int i = 0; i < 12; ++i) {
		const float ang = .5235994f * i;
		float cc = sinf(ang);
		float sc = cosf(ang);
		cairo_move_to(cr, cx + innercircle * sc, cy + innercircle * cc);
		cairo_line_to(cr, cx + radius * sc, cy + radius * cc);
		cairo_stroke (cr);
	}

#endif

#if 1 /* circular Level display */
	cairo_set_line_width(cr, 2.5);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

	const float cl = slow ? ui->ls : ui->lm;
	const float cm = slow ? ui->ms : ui->mm;
	bool maxed = false;
	for (float val = -69; val <= 0; val += 0.69) {
		if (val < cl) {
			radar_color(cr, val, -1);
		} else {
			cairo_set_source_rgba (cr, .3, .3, .3, 1.0);
		}
#if 0
		const float ang = .0682954f * val; // val * 2.0 * 3.0 * M_PI / 69.0 / 4.0;
		cairo_save(cr);
		cairo_translate (cr, cx, cy);
		cairo_rotate (cr, ang);
		cairo_translate (cr, radius + 10.0, 0);
		cairo_move_to(cr,  0.5, 0);
		if (!maxed && val >= cm && cm > -69) {
			radar_color(cr, val, -1);
			cairo_line_to(cr, 12.5, 0);
			maxed = true;
		} else {
			cairo_line_to(cr,  9.5, 0);
		}
		cairo_stroke (cr);
		cairo_restore(cr);
#else
		const float ang = .0682954f * val;
		float cc = sinf(ang);
		float sc = cosf(ang);
		cairo_move_to(cr, cx + (radius + 10) * sc, cy + (radius + 10)  * cc);
		if (!maxed && cm > -69 && (val >= cm || (val >= -.1 && cm >= 0))) {
			radar_color(cr, val, -1);
			cairo_line_to(cr, cx + (radius + 22) * sc, cy + (radius + 22) * cc);
			maxed = true;
		} else {
			cairo_line_to(cr, cx + (radius + 19) * sc, cy + (radius + 19) * cc);
		}
		cairo_stroke (cr);
#endif
	}

	sprintf(buf, "%+.0f", LUFS(-69));
	write_text(pc, cr, buf, "Mono 8", cx, cy + radius + 23, 0, 8, c_gry);

	sprintf(buf, "%+.0f", LUFS(-23));
	write_text(pc, cr, buf, "Mono 8", cx, cy - radius - 23, 0, 5, c_gry);

	sprintf(buf, "%+.0f", LUFS(0));
	write_text(pc, cr, buf, "Mono 8", cx + radius + 23, cy, 0, 3, c_gry);

	sprintf(buf, "%+.0f", LUFS(-46));
	write_text(pc, cr, buf, "Mono 8", cx - radius - 23, cy, 0, 1, c_gry);
#endif

	int myoff = 50;
	/* integrated level text display */
	if (ui->il > -60 || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->btn_start))) {
		cairo_set_source_rgba (cr, .1, .1, .1, 1.0);
		rounded_rectangle (cr, 15, wh-65, 40, 30, 10);
		cairo_fill (cr);
		write_text(pc, cr, "Long", "Sans 8", 35 , wh - 47, 0,  5, c_wht);

		cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
		rounded_rectangle (cr, 5, wh-45, ww-10, 40, 10);
		cairo_fill (cr);

		if (ui->il > -60) {
			sprintf(buf, "Int:   %+5.1f %s", LUFS(ui->il), lufs ? "LUFS" : "LU");
			write_text(pc, cr, buf, "Mono 9", 15 , wh - 25, 0,  6, c_wht);
		} else {
			sprintf(buf, "[Integrating over 5 sec]");
			write_text(pc, cr, buf, "Sans 9", 15 , wh - 25, 0,  6, c_wht);
		}

		if (ui->rx > -60 && (ui->rx - ui->rn) > 0) {
			sprintf(buf, "Range: %+5.1f..%+5.1f %s (%4.1f)",
					LUFS(ui->rn), LUFS(ui->rx), lufs ? "LUFS" : "LU", (ui->rx - ui->rn));
			write_text(pc, cr, buf, "Mono 9", 15 , wh - 10, 0,  6, c_wht);
		} else {
			sprintf(buf, "[10 sec range.. Please stand by]");
			write_text(pc, cr, buf, "Sans 9", 15 , wh - 10, 0,  6, c_wht);
		}
		myoff = 3;
	}

	/* bottom level text display */
	trw = lufs ? 117 : 105;
	cairo_set_source_rgba (cr, .1, .1, .1, 1.0);
	rounded_rectangle (cr, ww-55, 285+myoff, 40, 30, 10);
	cairo_fill (cr);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	rounded_rectangle (cr, ww-trw-5, 305+myoff, trw, 40, 10);
	cairo_fill (cr);

	write_text(pc, cr, slow? "Med":"Slow", "Sans 8", ww-35, 290+myoff, 0, 8, c_wht);
	sprintf(buf, "%+5.1f %s", LUFS(!slow? ui->ls : ui->lm), lufs ? "LUFS" : "LU");
	write_text(pc, cr, buf, "Mono 9", ww-15, 310+myoff, 0, 7, c_wht);
	sprintf(buf, "Max:%+5.1f %s", LUFS(!slow ? ui->ms: ui->mm), lufs ? "LUFS" : "LU");
	write_text(pc, cr, buf, "Mono 9", ww-15, 325+myoff, 0, 7, c_wht);


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
	gtk_widget_queue_draw(ui->m0);
	return TRUE;
}

static gboolean spn_radartime(GtkSpinButton *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	 float v = gtk_spin_button_get_value(w);
	forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_RADARTIME, v);
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

	ui->m0    = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), 330, 400);
	gtk_widget_set_size_request(ui->m0, 330, 400);

	ui->btn_box = gtk_hbox_new(TRUE, 0);
	ui->btn_start = gtk_toggle_button_new_with_label("start");
	ui->btn_reset = gtk_button_new_with_label("reset");

	ui->cbx_box = gtk_table_new(4, 3, FALSE);
	ui->cbx_lufs      = gtk_check_button_new_with_label("display LUFS");
	ui->cbx_slow      = gtk_check_button_new_with_label("'slow' main display");
	ui->cbx_transport = gtk_check_button_new_with_label("use host's transport");
	ui->cbx_autoreset = gtk_check_button_new_with_label("reset when starting");
	ui->spn_radartime = gtk_spin_button_new_with_range(30, 600, 15);
	ui->lbl_radarinfo = gtk_label_new("Radar Time:");
	ui->lbl_radarunit = gtk_label_new("sec");

	gtk_box_pack_start(GTK_BOX(ui->btn_box), ui->btn_start, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(ui->btn_box), ui->btn_reset, FALSE, FALSE, 2);

	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), ui->cbx_lufs     , 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), ui->cbx_slow     , 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), ui->cbx_transport, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), ui->cbx_autoreset, 0, 1, 3, 4);

	gtk_table_attach(GTK_TABLE(ui->cbx_box), ui->lbl_radarinfo, 1, 2, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), ui->spn_radartime, 1, 2, 2, 3);
	gtk_table_attach(GTK_TABLE(ui->cbx_box), ui->lbl_radarunit, 2, 3, 2, 3, GTK_SHRINK, GTK_SHRINK, 4, 0);

	gtk_box_pack_start(GTK_BOX(ui->box), ui->m0, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->cbx_box, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->btn_box, FALSE, FALSE, 2);

	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);
	g_signal_connect (G_OBJECT (ui->btn_start), "toggled", G_CALLBACK (btn_start), ui);
	g_signal_connect (G_OBJECT (ui->btn_reset), "clicked", G_CALLBACK (btn_reset), ui);
	g_signal_connect (G_OBJECT (ui->cbx_lufs),  "toggled", G_CALLBACK (cbx_lufs), ui);
	g_signal_connect (G_OBJECT (ui->cbx_slow),  "toggled", G_CALLBACK (cbx_lufs), ui);
	g_signal_connect (G_OBJECT (ui->cbx_transport), "toggled", G_CALLBACK (cbx_transport), ui);
	g_signal_connect (G_OBJECT (ui->cbx_autoreset), "toggled", G_CALLBACK (cbx_autoreset), ui);
	g_signal_connect (G_OBJECT (ui->spn_radartime), "value-changed", G_CALLBACK (spn_radartime), ui);

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
	if (ui->cpattern) {
		cairo_pattern_destroy (ui->cpattern);
	}
	free(ui);
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
				} else if (k == CTL_LV2_RADARTIME) {
					ui->disable_signals = true;
					gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_radartime), v);
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
