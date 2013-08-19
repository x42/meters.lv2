/* gtk dial widget
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

#include <gtk/gtk.h>
#include <cairo/cairo.h>

/* default values used by gtkext_dial_new()
 * for calling gtkext_dial_new_with_size()
 */
#define GED_WIDTH 55
#define GED_HEIGHT 29
#define GED_RADIUS 10
#define GED_CX 27.5
#define GED_CY 12.5

typedef struct {
	GtkWidget* w;
	GtkWidget* c;

	float min;
	float max;
	float acc;
	float cur;

	float drag_x, drag_y, drag_c;
	gboolean sensitive;

	gboolean (*cb) (GtkWidget* w, gpointer handle);
	gpointer handle;

	cairo_pattern_t* dpat;
	cairo_surface_t* bg;

	float w_width, w_height;
	float w_cx, w_cy;
	float w_radius;

} GtkExtDial;

static gboolean gtkext_dial_expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	GtkStyle *style = gtk_widget_get_style(w);
	GdkColor *c = &style->bg[GTK_STATE_NORMAL];
	cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill(cr);

	if (d->bg) {
		cairo_set_operator (cr, CAIRO_OPERATOR_EXCLUSION);
		cairo_set_source_surface(cr, d->bg, 0, 0);
		cairo_paint (cr);
		cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	}

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	if (d->sensitive) {
		cairo_set_source(cr, d->dpat);
	}
	cairo_arc (cr, d->w_cx, d->w_cy, d->w_radius, 0, 2.0 * M_PI);
	cairo_fill_preserve (cr);
	cairo_set_line_width(cr, .75);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_stroke (cr);

	if (d->sensitive) {
		cairo_set_source_rgba (cr, .95, .95, .95, 1.0);
	} else {
		cairo_set_source_rgba (cr, .5, .5, .5, .7);
	}
	cairo_set_line_width(cr, 1.5);
	cairo_move_to(cr, d->w_cx, d->w_cy);
	float ang = (.75 * M_PI) + (1.5 * M_PI) * (d->cur - d->min) / d->max;
	float wid = M_PI * 2 / 180.0;
	cairo_arc (cr, d->w_cx, d->w_cy, d->w_radius, ang-wid, ang+wid);
	cairo_stroke (cr);

	cairo_destroy (cr);
	return TRUE;
}

static void gtkext_dial_update_value(GtkExtDial * d, float val) {
	if (val < d->min) val = d->min;
	if (val > d->max) val = d->max;
	if (val != d->cur) {
		d->cur = val;
		if (d->cb) d->cb(d->w, d->handle);
		gtk_widget_queue_draw(d->w);
	}
}

static gboolean gtkext_dial_mousedown(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	if (!d->sensitive) { return FALSE; }
	d->drag_x = event->x;
	d->drag_y = event->y;
	d->drag_c = d->cur;
	gtk_widget_queue_draw(d->w);
	return TRUE;
}

static gboolean gtkext_dial_mouseup(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	if (!d->sensitive) { return FALSE; }
	d->drag_x = d->drag_y = -1;
	gtk_widget_queue_draw(d->w);
	return TRUE;
}

static gboolean gtkext_dial_mousemove(GtkWidget *w, GdkEventMotion *event, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	if (d->drag_x < 0 || d->drag_y < 0) return FALSE;

	if (!d->sensitive) {
		d->drag_x = d->drag_y = -1;
		gtk_widget_queue_draw(d->w);
		return FALSE;
	}

	float diff = ((event->x - d->drag_x) - (event->y - d->drag_y)) * 0.004; // 250px full-scale
	diff = rint(diff * (d->max - d->min) / d->acc ) * d->acc;
	float val = d->drag_c + diff;
	gtkext_dial_update_value(d, val);
	return TRUE;
}

static gboolean gtkext_dial_scroll(GtkWidget *w, GdkEventScroll *ev, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	if (!d->sensitive) { return FALSE; }

	if (!(d->drag_x < 0 || d->drag_y < 0)) {
		d->drag_x = d->drag_y = -1;
	}

	float val = d->cur;
	switch (ev->direction) {
		case GDK_SCROLL_RIGHT:
		case GDK_SCROLL_UP:
			val += d->acc;
			break;
		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_DOWN:
			val -= d->acc;
			break;
		default:
			break;
	}
	gtkext_dial_update_value(d, val);
	return TRUE;
}

static void create_dial_pattern(GtkExtDial * d) {
	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);

	const float pat_left   = (d->w_cx - d->w_radius) / (float) d->w_width;
	const float pat_right  = (d->w_cx + d->w_radius) / (float) d->w_width;
	const float pat_top    = (d->w_cy - d->w_radius) / (float) d->w_height;
	const float pat_bottom = (d->w_cy + d->w_radius) / (float) d->w_height;
#define PAT_XOFF(VAL) (pat_left + 0.35 * 2.0 * d->w_radius)

	cairo_pattern_add_color_stop_rgb (pat, pat_top,    .8, .8, .82);
	cairo_pattern_add_color_stop_rgb (pat, pat_bottom, .3, .3, .33);

	if (!getenv("NO_METER_SHADE") || strlen(getenv("NO_METER_SHADE")) == 0) {
		/* light from top-left */
		cairo_pattern_t* shade_pattern = cairo_pattern_create_linear (0.0, 0.0, d->w_width, 0.0);
		cairo_pattern_add_color_stop_rgba (shade_pattern, pat_left,       0.0, 0.0, 0.0, 0.15);
		cairo_pattern_add_color_stop_rgba (shade_pattern, PAT_XOFF(0.35), 1.0, 1.0, 1.0, 0.10);
		cairo_pattern_add_color_stop_rgba (shade_pattern, PAT_XOFF(0.53), 0.0, 0.0, 0.0, 0.05);
		cairo_pattern_add_color_stop_rgba (shade_pattern, pat_right,      0.0, 0.0, 0.0, 0.25);

		cairo_surface_t* surface;
		cairo_t* tc = 0;
		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, d->w_width, d->w_height);
		tc = cairo_create (surface);
		cairo_set_operator (tc, CAIRO_OPERATOR_SOURCE);
		cairo_set_source (tc, pat);
		cairo_rectangle (tc, 0, 0, d->w_width, d->w_height);
		cairo_fill (tc);
		cairo_pattern_destroy (pat);

		cairo_set_operator (tc, CAIRO_OPERATOR_OVER);
		cairo_set_source (tc, shade_pattern);
		cairo_rectangle (tc, 0, 0, d->w_width, d->w_height);
		cairo_fill (tc);
		cairo_pattern_destroy (shade_pattern);

		pat = cairo_pattern_create_for_surface (surface);
		cairo_destroy (tc);
		cairo_surface_destroy (surface);
	}

	d->dpat = pat;
}

/******************************************************************************
 * public functions
 */

static GtkExtDial * gtkext_dial_new_with_size(float min, float max, float step,
		int width, int height, float cx, float cy, float radius) {

	assert(max > min);
	assert( (max - min) / step <= 250.0);
	assert( (max - min) / step >= 1.0);

	assert( (cx  + radius) < width);
	assert( (cx  - radius) > 0);
	assert( (cy  + radius) < height);
	assert( (cy  - radius) > 0);

	GtkExtDial *d = (GtkExtDial *) malloc(sizeof(GtkExtDial));

	d->w_width = width; d->w_height = height;
	d->w_cx = cx; d->w_cy = cy;
	d->w_radius = radius;

	d->w = gtk_drawing_area_new();
	d->c = gtk_alignment_new(.5, .5, 0, 0);
	gtk_container_add(GTK_CONTAINER(d->c), d->w);

	d->cb = NULL;
	d->handle = NULL;
	d->min = min;
	d->max = max;
	d->acc = step;
	d->cur = min;
	d->sensitive = TRUE;
	d->bg  = NULL;
	create_dial_pattern(d);

	gtk_drawing_area_size(GTK_DRAWING_AREA(d->w), d->w_width, d->w_height);
	gtk_widget_set_size_request(d->w, d->w_width, d->w_height);

	gtk_widget_set_redraw_on_allocate(d->w, TRUE);
	gtk_widget_add_events(d->w, GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);

	g_signal_connect (G_OBJECT (d->w), "expose_event", G_CALLBACK (gtkext_dial_expose_event), d);
	g_signal_connect (G_OBJECT (d->w), "button-release-event", G_CALLBACK (gtkext_dial_mouseup), d);
	g_signal_connect (G_OBJECT (d->w), "button-press-event",   G_CALLBACK (gtkext_dial_mousedown), d);
	g_signal_connect (G_OBJECT (d->w), "motion-notify-event",  G_CALLBACK (gtkext_dial_mousemove), d);
	g_signal_connect (G_OBJECT (d->w), "scroll-event",  G_CALLBACK (gtkext_dial_scroll), d);

	return d;
}

static GtkExtDial * gtkext_dial_new(float min, float max, float step) {
	return gtkext_dial_new_with_size(min, max, step,
			GED_WIDTH, GED_HEIGHT, GED_CX, GED_CY, GED_RADIUS);
}

static void gtkext_dial_destroy(GtkExtDial *d) {
	gtk_widget_destroy(d->w);
	gtk_widget_destroy(d->c);
	cairo_pattern_destroy(d->dpat);
#if 0
	if (d->bg) { cairo_surface_destroy(d->bg); }
#endif
	free(d);
}

static GtkWidget * gtkext_dial_wiget(GtkExtDial *d) {
	return d->c;
}

static void gtkext_dial_set_callback(GtkExtDial *d, gboolean (*cb) (GtkWidget* w, gpointer handle), gpointer handle) {
	d->cb = cb;
	d->handle = handle;
}

static void gtkext_dial_set_value(GtkExtDial *d, float v) {
	v = d->min + rint((v-d->min) / d->acc ) * d->acc;
	gtkext_dial_update_value(d, v);
}

static void gtkext_dial_set_sensitive(GtkExtDial *d, gboolean s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		gtk_widget_queue_draw(d->w);
	}
}

static float gtkext_dial_get_value(GtkExtDial *d) {
	return (d->cur);
}

static void gtkext_dial_set_surface(GtkExtDial *d, cairo_surface_t *s) {
#if 0
	if (d->bg) { cairo_surface_destroy(d->bg); }
#endif
	d->bg = s;
}
