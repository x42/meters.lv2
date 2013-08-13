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
//#define DRAW_POINTS

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <cairo/cairo.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#include "jf.h"

#define JF_BOUNDS (205.0f)
#define JF_CENTER (102.5f)

#define JF_RADIUS (100.0f)
#define JR_RAD2   (50.0f)

/* CRT luminosity persistency
 * fade to FADE_ALPHA black every <sample-rate> / FADE_FREQ samples
 */
#define FADE_ALPHA (0.08)
#define FADE_FREQ  (30)

typedef struct {
	LV2_Handle instance;

	GtkWidget* box;
	GtkWidget* m0;

	cairo_surface_t* sf;
	cairo_surface_t* bg;

	float last_x, last_y;
	float lp0, lp1;
	float lpw;

	uint32_t fade_c;
	uint32_t fade_m;
} JFUI;

void alloc_sf(JFUI* ui) {
	ui->sf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, JF_BOUNDS, JF_BOUNDS);
	cairo_t* cr = cairo_create (ui->sf);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_rectangle (cr, 0, 0, JF_BOUNDS, JF_BOUNDS);
	cairo_fill (cr);
	cairo_destroy(cr);
}

void alloc_bg(JFUI* ui) {
	ui->bg = cairo_image_surface_create (CAIRO_FORMAT_RGB24, JF_BOUNDS, JF_BOUNDS);
	cairo_t* cr = cairo_create (ui->bg);

	/* background */
	cairo_rectangle (cr, 0, 0, JF_BOUNDS, JF_BOUNDS);
	cairo_set_source_rgba (cr, .1, .1, .1, 1.0);
	cairo_fill (cr);

	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS, 0, 2.0 * M_PI);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_fill(cr);

	/* rings */
	cairo_set_source_rgba (cr, .5, .5, .5, 1.0);

	cairo_set_line_width(cr, 1.5);
	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS, 0, 2.0 * M_PI);
	cairo_stroke (cr);

	cairo_set_line_width(cr, 1.5);
	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS *.707, 0, 2.0 * M_PI);
	cairo_stroke (cr);

	cairo_set_line_width(cr, 1.0);
	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS * 0.5, 0, 2.0 * M_PI);
	cairo_stroke (cr);

	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS * 0.354, 0, 2.0 * M_PI);
	cairo_stroke (cr);

	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS * 0.1257f, 0, 2.0 * M_PI);
	cairo_stroke (cr);

	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS * 0.089f, 0, 2.0 * M_PI);
	cairo_stroke (cr);

	/* cross */
	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS, 0, 2.0 * M_PI);
	cairo_clip(cr);

	cairo_set_source_rgba (cr, .6, .6, .6, 1.0);
	cairo_set_line_width(cr, 1.0);

	cairo_move_to(cr, 0.5, 0.5);
	cairo_line_to(cr, JF_BOUNDS - .5, JF_BOUNDS - .5);
	cairo_stroke (cr);

	cairo_move_to(cr, 0.5, JF_BOUNDS - .5);
	cairo_line_to(cr, JF_BOUNDS - .5, 0.5);
	cairo_stroke (cr);

	cairo_set_source_rgba (cr, .5, .5, .5, 1.0);

	cairo_move_to(cr, JF_CENTER, 0);
	cairo_line_to(cr, JF_CENTER, JF_BOUNDS);
	cairo_stroke (cr);

	cairo_move_to(cr, 0, JF_CENTER);
	cairo_line_to(cr, JF_BOUNDS, JF_CENTER);
	cairo_stroke (cr);

	cairo_destroy(cr);
}

void draw_rb(JFUI* ui, jfringbuf *rb) {
	cairo_t* cr = cairo_create (ui->sf);

	cairo_arc (cr, JF_CENTER, JF_CENTER, JF_RADIUS, 0, 2.0 * M_PI);
	cairo_clip(cr);

	//cairo_set_tolerance(cr, 1.0); // default .1

	cairo_set_source_rgba (cr, .0, 1.0, .0, 1.0);
	cairo_move_to(cr, ui->last_x, ui->last_y);

	size_t n_samples = jfrb_read_space(rb);
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
			cairo_rectangle (cr, 0, 0, JF_BOUNDS, JF_BOUNDS);
			cairo_fill (cr);
			cairo_move_to(cr, ui->last_x, ui->last_y);
			cairo_set_source_rgba (cr, .0, 1.0, .0, 1.0);
		}

		jfrb_read_one(rb, &d0, &d1);

#if 1
		/* low pass filter */
		ui->lp0 += ui->lpw * (d0 - ui->lp0);
		ui->lp1 += ui->lpw * (d1 - ui->lp1);
#else
		ui->lp0 = d0;
		ui->lp1 = d1;
#endif

		ui->last_x = JF_CENTER - (ui->lp0 - ui->lp1) * JR_RAD2;
		ui->last_y = JF_CENTER - (ui->lp0 + ui->lp1) * JR_RAD2;
#ifdef DRAW_POINTS
		cairo_move_to(cr, ui->last_x, ui->last_y);
		cairo_close_path (cr);
#else
		cairo_line_to(cr, ui->last_x, ui->last_y);
#endif
		cnt++;
	}

	if (cnt > 0) {
		cairo_stroke(cr);
	}

	cairo_destroy(cr);
}


static gboolean expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	JFUI* ui = (JFUI*)handle;
	LV2jf* self = (LV2jf*) ui->instance;

	draw_rb(ui, self->rb);

	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));

	cairo_set_source_surface(cr, ui->bg, 0, 0);
	cairo_paint (cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_ADD);
	cairo_set_source_surface(cr, ui->sf, 0, 0);
	cairo_paint (cr);

	cairo_destroy (cr);

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
	JFUI* ui = (JFUI*) calloc(1,sizeof(JFUI));
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

	LV2jf* self = (LV2jf*) ui->instance;

	alloc_sf(ui);
	alloc_bg(ui);

	ui->last_x = (JF_CENTER);
	ui->last_y = (JF_CENTER);
	ui->fade_c = 0;
	ui->fade_m = self->rate / FADE_FREQ;
	ui->lp0 = 0;
	ui->lp1 = 0;
	ui->lpw = expf(-2.0 * M_PI * 80 / self->rate);

	ui->box = gtk_alignment_new(.5, .5, 0, 0);
	ui->m0  = gtk_drawing_area_new();

	gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), JF_BOUNDS, JF_BOUNDS);
	gtk_widget_set_size_request(ui->m0, JF_BOUNDS, JF_BOUNDS);
	gtk_widget_set_redraw_on_allocate(ui->m0, TRUE);
	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);

	gtk_container_add(GTK_CONTAINER(ui->box), ui->m0);
	gtk_widget_show_all(ui->box);
	*widget = ui->box;

	jfrb_read_clear(self->rb);
	self->ui_active = true;
	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	JFUI* ui = (JFUI*)handle;
	LV2jf* i = (LV2jf*)ui->instance;

	i->ui_active = false;

	cairo_surface_destroy(ui->sf);
	cairo_surface_destroy(ui->bg);

	gtk_widget_destroy(ui->m0);

	free(ui);
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
	JFUI* ui = (JFUI*)handle;
	gtk_widget_queue_draw(ui->m0);
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
	"http://gareus.org/oss/lv2/meters#jfui",
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
