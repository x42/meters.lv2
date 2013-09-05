/* dial widget
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

#ifndef _ROB_TK_DIAL_H_
#define _ROB_TK_DIAL_H_

/* default values used by robtk_dial_new()
 * for calling robtk_dial_new_with_size()
 */
#define GED_WIDTH 55
#define GED_HEIGHT 30
#define GED_RADIUS 10
#define GED_CX 27.5
#define GED_CY 12.5

typedef struct {
	RobWidget *rw;

	float min;
	float max;
	float acc;
	float cur;
	float dfl;

	float drag_x, drag_y, drag_c;
	bool sensitive;
	bool prelight;

	bool (*cb) (RobWidget* w, void* handle);
	void* handle;

	cairo_pattern_t* dpat;
	cairo_surface_t* bg;

	float w_width, w_height;
	float w_cx, w_cy;
	float w_radius;

} RobTkDial;

static bool robtk_dial_expose_event (RobWidget* handle, cairo_t* cr, cairo_rectangle_t* ev) {
	RobTkDial * d = (RobTkDial *)GET_HANDLE(handle);
	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	float c[4];
	get_color_from_theme(1, c);
	cairo_set_source_rgb (cr, c[0], c[1], c[2]);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill(cr);

	if (d->bg) {
		if (!d->sensitive) {
			cairo_set_operator (cr, CAIRO_OPERATOR_EXCLUSION);
		} else {
			cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		}
		cairo_set_source_surface(cr, d->bg, 0, 0);
		cairo_paint (cr);
		cairo_set_source_rgb (cr, c[0], c[1], c[2]);
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
	float ang = (.75 * M_PI) + (1.5 * M_PI) * (d->cur - d->min) / (d->max - d->min);
	float wid = M_PI * 2 / 180.0;
	cairo_arc (cr, d->w_cx, d->w_cy, d->w_radius, ang-wid, ang+wid);
	cairo_stroke (cr);

	if (d->sensitive && (d->prelight || d->drag_x > 0)) {
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, .15);
		cairo_arc (cr, d->w_cx, d->w_cy, d->w_radius-1, 0, 2.0 * M_PI);
		cairo_fill(cr);
	}
	return TRUE;
}

static void robtk_dial_update_value(RobTkDial * d, float val) {
	if (val < d->min) val = d->min;
	if (val > d->max) val = d->max;
	if (val != d->cur) {
		d->cur = val;
		if (d->cb) d->cb(d->rw, d->handle);
		queue_draw(d->rw);
	}
}

static RobWidget* robtk_dial_mousedown(RobWidget* handle, RobTkBtnEvent *ev) {
	RobTkDial * d = (RobTkDial *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }
	if (ev->state & ROBTK_MOD_SHIFT) {
		robtk_dial_update_value(d, d->dfl);
	} else {
		d->drag_x = ev->x;
		d->drag_y = ev->y;
		d->drag_c = d->cur;
	}
	queue_draw(d->rw);
	return handle;
}

static RobWidget* robtk_dial_mouseup(RobWidget* handle, RobTkBtnEvent *ev) {
	RobTkDial * d = (RobTkDial *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }
	d->drag_x = d->drag_y = -1;
	queue_draw(d->rw);
	return NULL;
}

static RobWidget* robtk_dial_mousemove(RobWidget* handle, RobTkBtnEvent *ev) {
	RobTkDial * d = (RobTkDial *)GET_HANDLE(handle);
	if (d->drag_x < 0 || d->drag_y < 0) return NULL;

	if (!d->sensitive) {
		d->drag_x = d->drag_y = -1;
		queue_draw(d->rw);
		return NULL;
	}

	float diff = ((ev->x - d->drag_x) - (ev->y - d->drag_y)) * 0.004; // 250px full-scale
	diff = rint(diff * (d->max - d->min) / d->acc ) * d->acc;
	float val = d->drag_c + diff;
	robtk_dial_update_value(d, val);
	return handle;
}

static void robtk_dial_enter_notify(RobWidget *handle) {
	RobTkDial * d = (RobTkDial *)GET_HANDLE(handle);
	if (!d->prelight) {
		d->prelight = TRUE;
		queue_draw(d->rw);
	}
}

static void robtk_dial_leave_notify(RobWidget *handle) {
	RobTkDial * d = (RobTkDial *)GET_HANDLE(handle);
	if (d->prelight) {
		d->prelight = FALSE;
		queue_draw(d->rw);
	}
}


static RobWidget* robtk_dial_scroll(RobWidget* handle, RobTkBtnEvent *ev) {
	RobTkDial * d = (RobTkDial *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }

	if (!(d->drag_x < 0 || d->drag_y < 0)) {
		d->drag_x = d->drag_y = -1;
	}

	float val = d->cur;
	switch (ev->direction) {
		case ROBTK_SCROLL_RIGHT:
		case ROBTK_SCROLL_UP:
			val += d->acc;
			break;
		case ROBTK_SCROLL_LEFT:
		case ROBTK_SCROLL_DOWN:
			val -= d->acc;
			break;
		default:
			break;
	}
	robtk_dial_update_value(d, val);
	return NULL;
}

static void create_dial_pattern(RobTkDial * d) {
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
 * RobWidget stuff
 */

static void
robtk_dial_size_request(RobWidget* handle, int *w, int *h) {
	RobTkDial * d = (RobTkDial *)GET_HANDLE(handle);
	*w = d->w_width;
	*h = d->w_height;
}


/******************************************************************************
 * public functions
 */

static RobTkDial * robtk_dial_new_with_size(float min, float max, float step,
		int width, int height, float cx, float cy, float radius) {

	assert(max > min);
	assert(step > 0);
	assert( (max - min) / step <= 250.0);
	assert( (max - min) / step >= 1.0);

	assert( (cx  + radius) < width);
	assert( (cx  - radius) > 0);
	assert( (cy  + radius) < height);
	assert( (cy  - radius) > 0);

	RobTkDial *d = (RobTkDial *) malloc(sizeof(RobTkDial));

	d->w_width = width; d->w_height = height;
	d->w_cx = cx; d->w_cy = cy;
	d->w_radius = radius;

	d->rw = robwidget_new(d);
	ROBWIDGET_SETNAME(d->rw, "dial");
	robwidget_set_expose_event(d->rw, robtk_dial_expose_event);
	robwidget_set_size_request(d->rw, robtk_dial_size_request);
	robwidget_set_mouseup(d->rw, robtk_dial_mouseup);
	robwidget_set_mousedown(d->rw, robtk_dial_mousedown);
	robwidget_set_mousemove(d->rw, robtk_dial_mousemove);
	robwidget_set_mousescroll(d->rw, robtk_dial_scroll);
	robwidget_set_enter_notify(d->rw, robtk_dial_enter_notify);
	robwidget_set_leave_notify(d->rw, robtk_dial_leave_notify);

	d->cb = NULL;
	d->handle = NULL;
	d->min = min;
	d->max = max;
	d->acc = step;
	d->cur = min;
	d->dfl = min;
	d->sensitive = TRUE;
	d->prelight = FALSE;
	d->drag_x = d->drag_y = -1;
	d->bg  = NULL;
	create_dial_pattern(d);

	return d;
}

static RobTkDial * robtk_dial_new(float min, float max, float step) {
	return robtk_dial_new_with_size(min, max, step,
			GED_WIDTH, GED_HEIGHT, GED_CX, GED_CY, GED_RADIUS);
}

static void robtk_dial_destroy(RobTkDial *d) {
	robwidget_destroy(d->rw);
	cairo_pattern_destroy(d->dpat);
	free(d);
}

static void robtk_dial_set_alignment(RobTkDial *d, float x, float y) {
	robwidget_set_alignment(d->rw, x, y);
}

static RobWidget * robtk_dial_widget(RobTkDial *d) {
	return d->rw;
}

static void robtk_dial_set_callback(RobTkDial *d, bool (*cb) (RobWidget* w, void* handle), void* handle) {
	d->cb = cb;
	d->handle = handle;
}

static void robtk_dial_set_default(RobTkDial *d, float v) {
	v = d->min + rint((v-d->min) / d->acc ) * d->acc;
	assert(v >= d->min);
	assert(v <= d->max);
	d->dfl = v;
}

static void robtk_dial_set_value(RobTkDial *d, float v) {
	v = d->min + rint((v-d->min) / d->acc ) * d->acc;
	robtk_dial_update_value(d, v);
}

static void robtk_dial_set_sensitive(RobTkDial *d, bool s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		queue_draw(d->rw);
	}
}

static float robtk_dial_get_value(RobTkDial *d) {
	return (d->cur);
}

static void robtk_dial_set_surface(RobTkDial *d, cairo_surface_t *s) {
	d->bg = s;
}
#endif
