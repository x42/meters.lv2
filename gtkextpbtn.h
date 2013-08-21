/* gtk extended push button cairo widget
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

#ifndef _GTK_EXT_PBTN_H_
#define _GTK_EXT_PBTN_H_

#include <assert.h>
#include <gtk/gtk.h>
#include "common_cairo.h"

typedef struct {
	GtkWidget* w;
	GtkWidget* c;

	gboolean sensitive;
	gboolean prelight;
	gboolean enabled;

	gboolean (*cb) (GtkWidget* w, gpointer handle);
	gpointer handle;

	cairo_pattern_t* btn_active;
	cairo_pattern_t* btn_inactive;
	cairo_surface_t* sf_txt;

	float w_width, w_height;

} GtkExtPBtn;

static gboolean gtkext_pbtn_expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	GtkExtPBtn * d = (GtkExtPBtn *)handle;
	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	GtkStyle *style = gtk_widget_get_style(w);
	GdkColor *c = &style->bg[GTK_STATE_NORMAL];
	cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill(cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	if (!d->sensitive) {
		cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	} else if (d->enabled) {
		cairo_set_source(cr, d->btn_active);
	} else {
		cairo_set_source(cr, d->btn_inactive);
	}

	rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
	cairo_fill_preserve (cr);
	cairo_set_line_width (cr, .75);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_stroke(cr);

	if (d->enabled) {
		cairo_set_operator (cr, CAIRO_OPERATOR_XOR);
	} else {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	}
	cairo_set_source_surface(cr, d->sf_txt, 0, 0);
	cairo_paint (cr);

	if (d->sensitive && d->prelight) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, .1);
		rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
		cairo_fill_preserve(cr);
		cairo_set_line_width (cr, .75);
		cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
		cairo_stroke(cr);
	}
	cairo_destroy (cr);
	return TRUE;
}

static gboolean gtkext_pbtn_mousedown(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtPBtn * d = (GtkExtPBtn *)handle;
	if (!d->sensitive) { return FALSE; }
	if (!d->prelight) { return FALSE; }
	d->enabled = TRUE;
	gtk_widget_queue_draw(d->w);
	return TRUE;
}

static gboolean gtkext_pbtn_mouseup(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtPBtn * d = (GtkExtPBtn *)handle;
	if (!d->sensitive) { return FALSE; }
	if (d->prelight && d->enabled) {
		if (d->cb) d->cb(d->w, d->handle); // emit
	}
	d->enabled = FALSE;
	gtk_widget_queue_draw(d->w);
	return TRUE;
}

static gboolean gtkext_pbtn_enter_notify(GtkWidget *w, GdkEvent *event, gpointer handle) {
	GtkExtPBtn * d = (GtkExtPBtn *)handle;
	if (!d->prelight) {
		d->prelight = TRUE;
		gtk_widget_queue_draw(d->w);
	}
	return FALSE;
}
static gboolean gtkext_pbtn_leave_notify(GtkWidget *w, GdkEvent *event, gpointer handle) {
	GtkExtPBtn * d = (GtkExtPBtn *)handle;
	if (d->prelight) {
		d->prelight = FALSE;
		gtk_widget_queue_draw(d->w);
	}
	return FALSE;
}

static void create_pbtn_pattern(GtkExtPBtn * d) {
	d->btn_inactive = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);
	cairo_pattern_add_color_stop_rgb (d->btn_inactive, 0.0, .65, .65, .66);
	cairo_pattern_add_color_stop_rgb (d->btn_inactive, 1.0, .25, .25, .3);

	d->btn_active = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);
	cairo_pattern_add_color_stop_rgb (d->btn_active, 0.0, .3, .3, .33);
	cairo_pattern_add_color_stop_rgb (d->btn_active, 1.0, .8, .8, .82);
}

static void create_pbtn_text_surface(GtkExtPBtn * d, const char * txt, PangoFontDescription *font) {
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

	GdkColor color;
	get_color_from_gtk(&color, 0);
	float c_col[4];
	c_col[0] = color.red/65536.0;
	c_col[1] = color.green/65536.0;
	c_col[2] = color.blue/65536.0;
	c_col[3] = 1.0;

	write_text_full(cr, txt, font,
			d->w_width / 2.0 + 1,
			d->w_height / 2.0 + 1, 0, 2, c_col);
	cairo_destroy (cr);
}

/******************************************************************************
 * public functions
 */

static GtkExtPBtn * gtkext_pbtn_new(const char * txt) {
	assert(txt);
	GtkExtPBtn *d = (GtkExtPBtn *) malloc(sizeof(GtkExtPBtn));

	d->cb = NULL;
	d->handle = NULL;
	d->sf_txt = NULL;
	d->sensitive = TRUE;
	d->prelight = FALSE;
	d->enabled = FALSE;

	int ww, wh;
	PangoFontDescription *fd = get_font_from_gtk();

	get_text_geometry(txt, fd, &ww, &wh);
	d->w_width = ww + 14;
	d->w_height = wh + 8;

	create_pbtn_text_surface(d, txt, fd);
	pango_font_description_free(fd);

	d->w = gtk_drawing_area_new();
	d->c = gtk_alignment_new(.0, .5, 0, 0);
	gtk_container_add(GTK_CONTAINER(d->c), d->w);

	create_pbtn_pattern(d);

	gtk_drawing_area_size(GTK_DRAWING_AREA(d->w), d->w_width, d->w_height);
	gtk_widget_set_size_request(d->w, d->w_width, d->w_height);

	gtk_widget_set_redraw_on_allocate(d->w, TRUE);
	gtk_widget_add_events(d->w, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	g_signal_connect (G_OBJECT (d->w), "expose_event", G_CALLBACK (gtkext_pbtn_expose_event), d);
	g_signal_connect (G_OBJECT (d->w), "button-press-event", G_CALLBACK (gtkext_pbtn_mousedown), d);
	g_signal_connect (G_OBJECT (d->w), "button-release-event", G_CALLBACK (gtkext_pbtn_mouseup), d);
	g_signal_connect (G_OBJECT (d->w), "enter-notify-event",  G_CALLBACK (gtkext_pbtn_enter_notify), d);
	g_signal_connect (G_OBJECT (d->w), "leave-notify-event",  G_CALLBACK (gtkext_pbtn_leave_notify), d);

	return d;
}

static void gtkext_pbtn_destroy(GtkExtPBtn *d) {
	gtk_widget_destroy(d->w);
	gtk_widget_destroy(d->c);
	cairo_pattern_destroy(d->btn_active);
	cairo_pattern_destroy(d->btn_inactive);
	cairo_surface_destroy(d->sf_txt);
	free(d);
}

static void gtkext_pbtn_set_alignment(GtkExtPBtn *d, float x, float y) {
	gtk_alignment_set(GTK_ALIGNMENT(d->c), x, y, 0, 0);
}

static GtkWidget * gtkext_pbtn_widget(GtkExtPBtn *d) {
	return d->c;
}

static void gtkext_pbtn_set_callback(GtkExtPBtn *d, gboolean (*cb) (GtkWidget* w, gpointer handle), gpointer handle) {
	d->cb = cb;
	d->handle = handle;
}

static void gtkext_pbtn_set_sensitive(GtkExtPBtn *d, gboolean s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		gtk_widget_queue_draw(d->w);
	}
}
#endif
