/* gtk extended label cairo widget
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

#ifndef _GTK_EXT_LBL_H_
#define _GTK_EXT_LBL_H_

#include <assert.h>
#include <gtk/gtk.h>
#include "common_cairo.h"

typedef struct {
	GtkWidget* w;
	GtkWidget* c;

	gboolean sensitive;
	cairo_surface_t* sf_txt;
	float w_width, w_height;
} GtkExtLbl;

static gboolean gtkext_lbl_expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	GtkExtLbl * d = (GtkExtLbl *)handle;
	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	GtkStyle *style = gtk_widget_get_style(w);
	GdkColor *c = &style->bg[GTK_STATE_NORMAL];
	cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill(cr);

	if (d->sensitive) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	} else {
		cairo_set_operator (cr, CAIRO_OPERATOR_EXCLUSION);
	}
	cairo_set_source_surface(cr, d->sf_txt, 0, 0);
	cairo_paint (cr);

	cairo_destroy (cr);
	return TRUE;
}

static void create_lbl_text_surface(GtkExtLbl * d, const char * txt, PangoFontDescription *font) {
	if (d->sf_txt) {
		cairo_surface_destroy(d->sf_txt);
	}
	d->sf_txt = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, d->w_width, d->w_height);
	cairo_t *cr = cairo_create (d->sf_txt);
	cairo_set_source_rgba (cr, .0, .0, .0, 0);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	float c_col[4];
	get_cairo_color_from_gtk(0, c_col);

	write_text_full(cr, txt, font,
			d->w_width / 2.0 + 1,
			d->w_height / 2.0 + 1, 0, 2, c_col);
	cairo_destroy (cr);
}

/******************************************************************************
 * public functions
 */

static void gtkext_lbl_set_text(GtkExtLbl *d, const char *txt) {
	int ww, wh;
	PangoFontDescription *fd = get_font_from_gtk();

	get_text_geometry(txt, fd, &ww, &wh);
	d->w_width = ww + 4;
	d->w_height = wh + 4;

	create_lbl_text_surface(d, txt, fd);
	pango_font_description_free(fd);

	gtk_drawing_area_size(GTK_DRAWING_AREA(d->w), d->w_width, d->w_height);
	gtk_widget_set_size_request(d->w, d->w_width, d->w_height);
	gtk_widget_queue_draw(d->w);
}

static GtkExtLbl * gtkext_lbl_new(const char * txt) {
	assert(txt);
	GtkExtLbl *d = (GtkExtLbl *) malloc(sizeof(GtkExtLbl));

	d->sf_txt = NULL;
	d->sensitive = TRUE;
	d->w = gtk_drawing_area_new();
	d->c = gtk_alignment_new(.5, .5, 0, 0);
	gtk_container_add(GTK_CONTAINER(d->c), d->w);

	gtkext_lbl_set_text(d, txt);
	gtk_widget_set_redraw_on_allocate(d->w, TRUE);

	g_signal_connect (G_OBJECT (d->w), "expose_event", G_CALLBACK (gtkext_lbl_expose_event), d);

	return d;
}

static void gtkext_lbl_destroy(GtkExtLbl *d) {
	gtk_widget_destroy(d->w);
	gtk_widget_destroy(d->c);
	cairo_surface_destroy(d->sf_txt);
	free(d);
}

static void gtkext_lbl_set_alignment(GtkExtLbl *d, float x, float y) {
	gtk_alignment_set(GTK_ALIGNMENT(d->c), x, y, 0, 0);
}

static GtkWidget * gtkext_lbl_widget(GtkExtLbl *d) {
	return d->c;
}

static void gtkext_lbl_set_sensitive(GtkExtLbl *d, gboolean s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		gtk_widget_queue_draw(d->w);
	}
}
#endif
