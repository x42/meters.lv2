/* goniometer LV2 GUI
 *
 * Copyright 2012-2013 Robin Gareus <robin@gareus.org>
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
#define DRAW_POINTS

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <pango/pango.h>

typedef void Stcorrdsp;

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#include "goniometer.h"

#define PC_BOUNDS ( 40.0f)

#define PC_TOP    ( 12.5f)
#define PC_BLOCK  (  8.0f)
#define PC_LEFT   ( 10.0f)
#define PC_WIDTH  ( 20.0f)
#define PC_HEIGHT (380.0f)

#define GM_BOUNDS (405.0f)
#define GM_CENTER (202.5f)

#define GM_RADIUS (200.0f)
#define GM_RAD2   (100.0f)

/* CRT luminosity persistency
 * fade to FADE_ALPHA black every <sample-rate> / FADE_FREQ samples
 */
#define FADE_ALPHA (0.22)
#define FADE_FREQ  (30)

#define MAX_CAIRO_PATH 100

typedef struct {
	LV2_Handle instance;
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	GtkWidget* box;
	GtkWidget* align;
	GtkWidget* fader;
	GtkWidget* m0;

	cairo_surface_t* sf;
	cairo_surface_t* an[7];

	float last_x, last_y;
	float lp0, lp1;
	float lpw;

	float cor, cor_u;
	uint32_t ntfy_u, ntfy_b;

	uint32_t fade_c;
	uint32_t fade_m;

	float gain;
	bool disable_signals;
} GMUI;

static void write_text(
		cairo_t* cr,
		const char *txt, const char * font,
		const float x, const float y) {
	int tw, th;
	cairo_save(cr);

	PangoLayout * pl = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string(font);
	pango_layout_set_font_description(pl, fd);
	pango_font_description_free(fd);
	cairo_set_source_rgba (cr, .5, .5, .6, 1.0);
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, &tw, &th);
	cairo_translate (cr, x, y);
	cairo_translate (cr, -tw/2.0 - 0.5, -th/2.0);
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
  cairo_arc (cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);
  cairo_arc (cr, x + r, y + h - r, r, 90 * degrees, 180 * degrees);
  cairo_arc (cr, x + r, y + r, r, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);
}

static void alloc_annotations(GMUI* ui) {
#define FONT_GM "Mono 16"
#define FONT_PC "Mono 10"

#define INIT_BLACK_BG(ID, WIDTH, HEIGHT) \
	ui->an[ID] = cairo_image_surface_create (CAIRO_FORMAT_RGB24, WIDTH, HEIGHT); \
	cr = cairo_create (ui->an[ID]); \
	cairo_set_source_rgb (cr, .0, .0, .0); \
	cairo_rectangle (cr, 0, 0, WIDTH, WIDTH); \
	cairo_fill (cr);

	cairo_t* cr;

	INIT_BLACK_BG(0, 32, 32)
	write_text(cr, "L", FONT_GM, 16, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(1, 32, 32)
	write_text(cr, "R", FONT_GM, 16, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(2, 64, 32)
	write_text(cr, "Mono", FONT_GM, 32, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(3, 32, 32)
	write_text(cr, "+S", FONT_GM, 16, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(4, 32, 32)
	write_text(cr, "-S", FONT_GM, 16, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(5, 32, 32)
	write_text(cr, "+1", FONT_PC, 10, 10);
	cairo_destroy (cr);

	INIT_BLACK_BG(6, 32, 32)
	write_text(cr, "-1", FONT_PC, 10, 10);
	cairo_destroy (cr);
}

static void alloc_sf(GMUI* ui) {
	ui->sf = cairo_image_surface_create (CAIRO_FORMAT_RGB24, GM_BOUNDS, GM_BOUNDS);
	cairo_t* cr = cairo_create (ui->sf);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_rectangle (cr, 0, 0, GM_BOUNDS, GM_BOUNDS);
	cairo_fill (cr);
	cairo_destroy(cr);
}


static void draw_rb(GMUI* ui, gmringbuf *rb) {
	cairo_t* cr = cairo_create (ui->sf);

	cairo_rectangle (cr, 0, 0, GM_BOUNDS, GM_BOUNDS);
	cairo_clip(cr);

	cairo_set_source_rgba (cr, .8, .8, .2, 1.0);
	cairo_move_to(cr, ui->last_x, ui->last_y);

	size_t n_samples = gmrb_read_space(rb);
	float d0, d1;

	int cnt = 0;
#ifdef DRAW_POINTS
	cairo_set_line_width(cr, 1.5);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
#else
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, ui->last_x, ui->last_y);
#endif
	for (uint32_t i=0; i < n_samples; ++i, ++ui->fade_c) {

		if (ui->fade_c > ui->fade_m) {
			ui->fade_c = 0;

			if (cnt > 0) {
				cairo_stroke(cr);
				cnt = 0;
			}

			/* fade luminosity */
			cairo_set_source_rgba (cr, .0, .0, .0, FADE_ALPHA);
			cairo_rectangle (cr, 0, 0, GM_BOUNDS, GM_BOUNDS);
			cairo_fill (cr);
			cairo_move_to(cr, ui->last_x, ui->last_y);
			cairo_set_source_rgba (cr, .8, .8, .2, 1.0);
		}

		if (gmrb_read_one(rb, &d0, &d1)) break;

#if 1
		/* low pass filter */
		ui->lp0 += ui->lpw * (d0 - ui->lp0);
		ui->lp1 += ui->lpw * (d1 - ui->lp1);
#else
		ui->lp0 = d0;
		ui->lp1 = d1;
#endif

		ui->last_x = GM_CENTER - ui->gain * (ui->lp0 - ui->lp1) * GM_RAD2;
		ui->last_y = GM_CENTER - ui->gain * (ui->lp0 + ui->lp1) * GM_RAD2;
#ifdef DRAW_POINTS
		cairo_move_to(cr, ui->last_x, ui->last_y);
		cairo_close_path (cr);
#else
		cairo_line_to(cr, ui->last_x, ui->last_y);
#endif
		if (++cnt > MAX_CAIRO_PATH) {
			cnt = 0;
			cairo_stroke(cr);
		}
	}

	if (cnt > 0) {
		cairo_stroke(cr);
	}

	cairo_destroy(cr);
}

static void draw_gm_labels(GMUI* ui, cairo_t* cr) {
	cairo_save(cr);
	cairo_translate(cr, PC_BOUNDS, 0);
	cairo_set_operator (cr, CAIRO_OPERATOR_SCREEN);

#define DRAW_LABEL(ID, XPOS, YPOS) \
	cairo_set_source_surface(cr, ui->an[ID], (XPOS)-16, (YPOS)-16); cairo_paint (cr);

	DRAW_LABEL(0, GM_CENTER - GM_RAD2, GM_CENTER - GM_RAD2)
	DRAW_LABEL(1, GM_CENTER + GM_RAD2, GM_CENTER - GM_RAD2);

	DRAW_LABEL(2, GM_CENTER - 16, GM_CENTER - GM_RADIUS * 3/4 - 12);
	DRAW_LABEL(3, GM_CENTER - GM_RADIUS * 3/4 - 12 , GM_CENTER - 1);
	DRAW_LABEL(4, GM_CENTER + GM_RADIUS * 3/4 + 12 , GM_CENTER - 1);

	const double dashed[] = {1.0, 2.0};
	cairo_set_line_width(cr, 3.5);
	cairo_set_source_rgba (cr, .5, .5, .6, 1.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

	cairo_set_dash(cr, dashed, 2, 0);
	cairo_move_to(cr, GM_CENTER - GM_RADIUS * 0.7079, GM_CENTER);
	cairo_line_to(cr, GM_CENTER + GM_RADIUS * 0.7079, GM_CENTER);
	cairo_stroke(cr);

	cairo_move_to(cr, GM_CENTER, GM_CENTER - GM_RADIUS * 0.7079);
	cairo_line_to(cr, GM_CENTER, GM_CENTER + GM_RADIUS * 0.7079);
	cairo_stroke(cr);

	cairo_restore(cr);
}

static void draw_pc_annotation(GMUI* ui, cairo_t* cr) {
	cairo_set_operator (cr, CAIRO_OPERATOR_SCREEN);
	cairo_set_source_rgba (cr, .5, .5, .6, 1.0);
	cairo_set_line_width(cr, 1.5);

#define PC_ANNOTATION(YPOS) \
	cairo_move_to(cr, PC_LEFT + 2.0, PC_TOP + YPOS); \
	cairo_line_to(cr, PC_LEFT + PC_WIDTH - 2.0, PC_TOP + YPOS);\
	cairo_stroke(cr);

	PC_ANNOTATION(PC_HEIGHT * 0.1);
	PC_ANNOTATION(PC_HEIGHT * 0.2);
	PC_ANNOTATION(PC_HEIGHT * 0.3);
	PC_ANNOTATION(PC_HEIGHT * 0.4);
	PC_ANNOTATION(PC_HEIGHT * 0.6);
	PC_ANNOTATION(PC_HEIGHT * 0.7);
	PC_ANNOTATION(PC_HEIGHT * 0.8);
	PC_ANNOTATION(PC_HEIGHT * 0.9);

	DRAW_LABEL(5, PC_LEFT + 16, PC_TOP + 14);
	DRAW_LABEL(6, PC_LEFT + 16, PC_TOP + PC_HEIGHT - 2);

	cairo_set_line_width(cr, 2.0);
	cairo_set_source_rgba (cr, .7, .7, .8, 1.0);
	PC_ANNOTATION(PC_HEIGHT * 0.5);
}

static gboolean expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	LV2gm* self = (LV2gm*) ui->instance;

	/* process and draw goniometer data */
	if (ui->ntfy_b != ui->ntfy_u) {
		ui->ntfy_u = ui->ntfy_b;
		draw_rb(ui, self->rb);
	}

	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	/* display goniometer */
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, ui->sf, PC_BOUNDS, 0);
	cairo_paint (cr);
	draw_gm_labels(ui, cr);

	/* display phase-correlation */

	/* PC meter backgroud */
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	cairo_rectangle (cr, 0, 0, PC_BOUNDS, GM_BOUNDS);
	cairo_fill(cr);

	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_set_line_width(cr, 1.0);
	rounded_rectangle (cr, PC_LEFT -1.0, PC_TOP - 2.0 , PC_WIDTH + 2.0, PC_HEIGHT + 4.0, 6);
	cairo_fill(cr);

	/* value */
	cairo_set_source_rgba (cr, .8, .8, .2, 1.0);
	const float c = (PC_HEIGHT - PC_BLOCK) * ui->cor;
	rounded_rectangle (cr, PC_LEFT, PC_TOP + c, PC_WIDTH, PC_BLOCK, 2);
	cairo_fill(cr);

	draw_pc_annotation(ui, cr);

	cairo_destroy (cr);

	return TRUE;
}

static gboolean set_gain(GtkRange* r, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	ui->gain = gtk_range_get_value(r);
	if (!ui->disable_signals) {
		ui->write(ui->controller, 4, sizeof(float), 0, (const void*) &ui->gain);
	}
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
	GMUI* ui = (GMUI*) calloc(1,sizeof(GMUI));
	*widget = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, "http://lv2plug.in/ns/ext/instance-access")) {
			ui->instance = (LV2_Handle*)features[i]->data;
		}
	}

	if (!ui->instance) {
		fprintf(stderr, "meters.lv2 error: Host does not support instance-access\n");
		free(ui);
		return NULL;
	}

	LV2gm* self = (LV2gm*) ui->instance;

	ui->write      = write_function;
	ui->controller = controller;

	alloc_sf(ui);
	alloc_annotations(ui);

	ui->last_x = (GM_CENTER);
	ui->last_y = (GM_CENTER);
	ui->fade_c = 0;
	ui->fade_m = self->rate / FADE_FREQ;
	ui->lp0 = 0;
	ui->lp1 = 0;
	ui->lpw = expf(-2.0 * M_PI * 80 / self->rate);
	ui->gain = 1.0;

	ui->cor = 0.5;
	ui->cor_u = 0.5;
	ui->ntfy_b = 0;
	ui->ntfy_u = 1;
	ui->disable_signals = false;

	ui->box   = gtk_vbox_new(FALSE, 2);
	ui->align = gtk_alignment_new(.5, .5, 0, 0);
	ui->m0    = gtk_drawing_area_new();
	ui->fader = gtk_hscale_new_with_range(0, 6.0, .001);

	gtk_scale_set_draw_value(GTK_SCALE(ui->fader), FALSE);
	gtk_range_set_value(GTK_RANGE(ui->fader), 1.0);
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 5.6234, GTK_POS_BOTTOM, "+15dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 3.9810, GTK_POS_TOP, "+12dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 2.8183, GTK_POS_BOTTOM, "+9dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 1.9952, GTK_POS_TOP, "+6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 1.4125, GTK_POS_BOTTOM, "+3dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 1.0000, GTK_POS_TOP, "0dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.7079, GTK_POS_BOTTOM, "-3dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.5012, GTK_POS_TOP, "-6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.3548, GTK_POS_BOTTOM, "-9dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.2511, GTK_POS_TOP, "-12dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.1778, GTK_POS_BOTTOM, "-15dB");
	//gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.0,  GTK_POS_BOTTOM, ""); // -inf

	gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), PC_BOUNDS + GM_BOUNDS, GM_BOUNDS);
	gtk_widget_set_size_request(ui->m0, PC_BOUNDS + GM_BOUNDS, GM_BOUNDS);
	gtk_widget_set_redraw_on_allocate(ui->m0, TRUE);

	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);
	g_signal_connect (G_OBJECT (ui->fader), "value-changed", G_CALLBACK (set_gain), ui);

	gtk_container_add(GTK_CONTAINER(ui->align), ui->m0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->align, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->fader, FALSE, FALSE, 0);

	gtk_widget_show_all(ui->box);
	*widget = ui->box;

	gmrb_read_clear(self->rb);
	self->ui_active = true;
	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	GMUI* ui = (GMUI*)handle;
	LV2gm* i = (LV2gm*)ui->instance;

	i->ui_active = false;

	cairo_surface_destroy(ui->sf);
	for (int i=0; i < 7 ; ++i) {
		cairo_surface_destroy(ui->an[i]);
	}
	gtk_widget_destroy(ui->m0);

	free(ui);
}

static void invalidate_gm(GMUI* ui) {
	gtk_widget_queue_draw_area(ui->m0, PC_BOUNDS, 0, GM_BOUNDS, GM_BOUNDS);
}

static void invalidate_pc(GMUI* ui) {
	float c;
	c = (PC_HEIGHT - PC_BLOCK) * ui->cor_u;
	gtk_widget_queue_draw_area(ui->m0, PC_LEFT, PC_TOP + c -1 , PC_WIDTH, PC_BLOCK + 2);
	ui->cor_u = ui->cor;
	c = (PC_HEIGHT - PC_BLOCK) * ui->cor_u;
	gtk_widget_queue_draw_area(ui->m0, PC_LEFT, PC_TOP + c -1 , PC_WIDTH, PC_BLOCK + 2);
}

/******************************************************************************
 * handle data from backend
 */

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	GMUI* ui = (GMUI*)handle;
	if (format != 0) return;
	if (port_index == 4) {
		float v = *(float *)buffer;
		if (v >= 0 && v <= 6.0) {
			ui->disable_signals = true;
			gtk_range_set_value(GTK_RANGE(ui->fader), v);
			ui->disable_signals = false;
		}
	} else
	if (port_index == 5) {
		ui->cor = 0.5f * (1.0f - *(float *)buffer);
		invalidate_pc(ui);
	} else
	if (port_index == 6) {
		ui->ntfy_b = (uint32_t) (*(float *)buffer);
		invalidate_gm(ui);
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
	"http://gareus.org/oss/lv2/meters#goniometerui",
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
