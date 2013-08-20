/* gtk extended radio button cairo widget
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

#ifndef _GTK_EXT_CBTN_H_
#define _GTK_EXT_CBTN_H_

#include <assert.h>
#include <gtk/gtk.h>
#include "common_cairo.h"

#define GBT_LED_RADIUS (11.0)
enum GedLedMode {
	GBT_LED_RADIO  = -2,
	GBT_LED_LEFT  = -1,
	GBT_LED_OFF   = 0,
	GBT_LED_RIGHT = 1
};

typedef struct {
	GtkWidget* w;
	GtkWidget* c;

	gboolean sensitive;
	gboolean prelight;
	gboolean enabled;

	enum GedLedMode show_led;
	gboolean flat_button;
	gboolean radiomode;

	gboolean (*cb) (GtkWidget* w, gpointer handle);
	gpointer handle;

	cairo_pattern_t* btn_active;
	cairo_pattern_t* btn_inactive;
	cairo_pattern_t* btn_led;
	cairo_surface_t* sf_txt;

	float w_width, w_height;

} GtkExtCBtn;

static gboolean gtkext_cbtn_expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	GtkExtCBtn * d = (GtkExtCBtn *)handle;
	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);
	float led_r, led_g, led_b;

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	GtkStyle *style = gtk_widget_get_style(w);
	GdkColor *c = &style->bg[GTK_STATE_NORMAL];
	cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill(cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	if (!d->sensitive) {
			led_r = c->red/65536.0;
			led_g = c->green/65536.0;
			led_b = c->blue/65536.0;
		} else if (d->enabled) {
			if (d->radiomode) {
				led_r = .3; led_g = .8; led_b = .1;
			} else {
				led_r = .8; led_g = .3; led_b = .1;
			}
		} else {
			if (d->radiomode) {
				led_r = .1; led_g = .3; led_b = .1;
			} else {
				led_r = .3; led_g = .1; led_b = .1;
			}
		}


	if (!d->flat_button) {
		if (!d->sensitive) {
			cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
			led_r = c->red/65536.0;
			led_g = c->green/65536.0;
			led_b = c->blue/65536.0;
		} else if (d->enabled) {
			cairo_set_source(cr, d->btn_active);
			led_r = .8;
			led_g = .3;
			led_b = .1;
		} else {
			cairo_set_source(cr, d->btn_inactive);
			led_r = .3;
			led_g = .1;
			led_b = .1;
		}

		rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
		cairo_fill_preserve (cr);
		cairo_set_line_width (cr, .75);
		cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
		cairo_stroke(cr);
	}

	if (d->flat_button && !d->sensitive) {
		//cairo_set_operator (cr, CAIRO_OPERATOR_XOR); // check
		cairo_set_operator (cr, CAIRO_OPERATOR_EXCLUSION);
	} else {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	}
	cairo_set_source_surface(cr, d->sf_txt, 0, 0);
	cairo_paint (cr);


	if (d->show_led) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_save(cr);
		if (d->show_led == GBT_LED_LEFT || d->show_led == GBT_LED_RADIO) {
			cairo_translate(cr, GBT_LED_RADIUS/2 + 7, d->w_height/2.0 + 1);
		} else {
			cairo_translate(cr, d->w_width - GBT_LED_RADIUS/2 - 7, d->w_height/2.0 + 1);
		}
		cairo_set_source (cr, d->btn_led);
		cairo_arc (cr, 0, 0, GBT_LED_RADIUS/2, 0, 2 * M_PI);
		cairo_fill(cr);

		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_arc (cr, 0, 0, GBT_LED_RADIUS/2 - 2, 0, 2 * M_PI);
		cairo_fill(cr);
		cairo_set_source_rgba (cr, led_r, led_g, led_b, 1.0);
		cairo_arc (cr, 0, 0, GBT_LED_RADIUS/2 - 3, 0, 2 * M_PI);
		cairo_fill(cr);
		cairo_restore(cr);
	}


	if (d->sensitive && d->prelight) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, .1);
		if (d->flat_button) {
			rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
			//cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
			cairo_fill(cr);
		} else {
			rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
			cairo_fill_preserve(cr);
			cairo_set_line_width (cr, .75);
			cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
			cairo_stroke(cr);
		}
	}
	cairo_destroy (cr);
	return TRUE;
}

static void gtkext_cbtn_update_enabled(GtkExtCBtn * d, gboolean enabled) {
	if (enabled != d->enabled) {
		d->enabled = enabled;
		if (d->cb) d->cb(d->w, d->handle);
		gtk_widget_queue_draw(d->w);
	}
}

static gboolean gtkext_cbtn_mouseup(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtCBtn * d = (GtkExtCBtn *)handle;
	if (!d->sensitive) { return FALSE; }
	if (!d->prelight) { return FALSE; }
	if (d->radiomode && d->enabled) { return FALSE; }
	gtkext_cbtn_update_enabled(d, ! d->enabled);
	return TRUE;
}

static gboolean gtkext_cbtn_enter_notify(GtkWidget *w, GdkEvent *event, gpointer handle) {
	GtkExtCBtn * d = (GtkExtCBtn *)handle;
	if (!d->prelight) {
		d->prelight = TRUE;
		gtk_widget_queue_draw(d->w);
	}
	return FALSE;
}
static gboolean gtkext_cbtn_leave_notify(GtkWidget *w, GdkEvent *event, gpointer handle) {
	GtkExtCBtn * d = (GtkExtCBtn *)handle;
	if (d->prelight) {
		d->prelight = FALSE;
		gtk_widget_queue_draw(d->w);
	}
	return FALSE;
}

static void create_cbtn_pattern(GtkExtCBtn * d) {
	d->btn_inactive = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);
	cairo_pattern_add_color_stop_rgb (d->btn_inactive, 0.0, .3, .3, .3);
	cairo_pattern_add_color_stop_rgb (d->btn_inactive, 1.0, .2, .2, .2);

	d->btn_active = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);
	cairo_pattern_add_color_stop_rgb (d->btn_active, 0.0, .3, .3, .3);
	cairo_pattern_add_color_stop_rgb (d->btn_active, 1.0, .4, .4, .4);

	d->btn_led = cairo_pattern_create_linear (0.0, 0.0, 0.0, GBT_LED_RADIUS);
	cairo_pattern_add_color_stop_rgba (d->btn_led, 0.0, 0.0, 0.0, 0.0, 0.4);
	cairo_pattern_add_color_stop_rgba (d->btn_led, 1.0, 1.0, 1.0, 1.0 , 0.7);
}

static void create_text_surface(GtkExtCBtn * d, const char * txt, PangoFontDescription *font) {
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
			(d->w_width - (d->show_led ? GBT_LED_RADIUS + 6 : 0)) / 2.0
			 + 1
			//- (d->show_led == GBT_LED_RADIO ? 5 : 0)
			 + (d->show_led < 0 ? GBT_LED_RADIUS + 6 : 0),
			d->w_height / 2.0 + 1, 0, 2, c_col);
	cairo_destroy (cr);
}

/******************************************************************************
 * public functions
 */

static GtkExtCBtn * gtkext_cbtn_new(const char * txt, enum GedLedMode led, gboolean flat) {
	assert(txt);
	GtkExtCBtn *d = (GtkExtCBtn *) malloc(sizeof(GtkExtCBtn));

	d->flat_button = flat;
	d->show_led = led;
	d->cb = NULL;
	d->handle = NULL;
	d->sf_txt = NULL;
	d->sensitive = TRUE;
	d->radiomode = FALSE;
	d->prelight = FALSE;
	d->enabled = FALSE;

	if (led == GBT_LED_RADIO) {
		d->radiomode = TRUE;
	}

	int ww, wh;
	//PangoFontDescription *fd = pango_font_description_from_string("Sans 9");
	PangoFontDescription *fd = get_font_from_gtk();

	get_text_geometry(txt, fd, &ww, &wh);
	d->w_width = ww + 14 + (d->show_led ? GBT_LED_RADIUS + 6 : 0);
	d->w_height = wh + 8;

	create_text_surface(d, txt, fd);
	pango_font_description_free(fd);

	d->w = gtk_drawing_area_new();
	d->c = gtk_alignment_new(0, .5, 0, 0);
	gtk_container_add(GTK_CONTAINER(d->c), d->w);

	create_cbtn_pattern(d);

	gtk_drawing_area_size(GTK_DRAWING_AREA(d->w), d->w_width, d->w_height);
	gtk_widget_set_size_request(d->w, d->w_width, d->w_height);

	gtk_widget_set_redraw_on_allocate(d->w, TRUE);
	gtk_widget_add_events(d->w, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	g_signal_connect (G_OBJECT (d->w), "expose_event", G_CALLBACK (gtkext_cbtn_expose_event), d);
	g_signal_connect (G_OBJECT (d->w), "button-release-event", G_CALLBACK (gtkext_cbtn_mouseup), d);
	g_signal_connect (G_OBJECT (d->w), "enter-notify-event",  G_CALLBACK (gtkext_cbtn_enter_notify), d);
	g_signal_connect (G_OBJECT (d->w), "leave-notify-event",  G_CALLBACK (gtkext_cbtn_leave_notify), d);

	return d;
}

static void gtkext_cbtn_destroy(GtkExtCBtn *d) {
	gtk_widget_destroy(d->w);
	gtk_widget_destroy(d->c);
	cairo_pattern_destroy(d->btn_active);
	cairo_pattern_destroy(d->btn_inactive);
	cairo_pattern_destroy(d->btn_led);
	cairo_surface_destroy(d->sf_txt);
	free(d);
}

static GtkWidget * gtkext_cbtn_widget(GtkExtCBtn *d) {
	return d->c;
}

static void gtkext_cbtn_set_callback(GtkExtCBtn *d, gboolean (*cb) (GtkWidget* w, gpointer handle), gpointer handle) {
	d->cb = cb;
	d->handle = handle;
}

static void gtkext_cbtn_set_active(GtkExtCBtn *d, gboolean v) {
	gtkext_cbtn_update_enabled(d, v);
}

static void gtkext_cbtn_set_sensitive(GtkExtCBtn *d, gboolean s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		gtk_widget_queue_draw(d->w);
	}
}

static gboolean gtkext_cbtn_get_active(GtkExtCBtn *d) {
	return (d->enabled);
}
#endif
