/* push button widget
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

#ifndef _ROB_TK_PBTN_H_
#define _ROB_TK_PBTN_H_

typedef struct {
	RobWidget* rw;

	bool sensitive;
	bool prelight;
	bool enabled;

	bool (*cb) (RobWidget* w, void* handle);
	void* handle;
	bool (*cb_up) (RobWidget* w, void* handle);
	void* handle_up;
	bool (*cb_down) (RobWidget* w, void* handle);
	void* handle_down;

	cairo_pattern_t* btn_active;
	cairo_pattern_t* btn_inactive;
	cairo_surface_t* sf_txt;

	float w_width, w_height, l_width;

} RobTkPBtn;

static bool robtk_pbtn_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t* ev) {
	RobTkPBtn * d = (RobTkPBtn *)GET_HANDLE(handle);
	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	if (handle->area.width > d->w_width) {
		d->w_width = handle->area.width;
	}

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

	float c[4];
	get_color_from_theme(1, c);
	cairo_set_source_rgb (cr, c[0], c[1], c[2]);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill(cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	if (!d->sensitive) {
	cairo_set_source_rgb (cr, c[0], c[1], c[2]);
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
	cairo_set_source_surface(cr, d->sf_txt, rint((d->w_width - d->l_width) / 2.0), 0);
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
	return TRUE;
}

/******************************************************************************
 * UI callbacks
 */

static RobWidget* robtk_pbtn_mousedown(RobWidget *handle, RobTkBtnEvent *event) {
	RobTkPBtn * d = (RobTkPBtn *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }
	if (!d->prelight) { return NULL; }
	d->enabled = TRUE;
	if (d->cb_down) d->cb_down(d->rw, d->handle_down);
	queue_draw(d->rw);
	return handle;
}

static RobWidget* robtk_pbtn_mouseup(RobWidget *handle, RobTkBtnEvent *event) {
	RobTkPBtn * d = (RobTkPBtn *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }
	if (d->enabled && d->cb_up) {
		d->cb_up(d->rw, d->handle_up);
	}
	if (d->prelight && d->enabled) {
		if (d->cb) d->cb(d->rw, d->handle);
	}
	d->enabled = FALSE;
	queue_draw(d->rw);
	return NULL;
}

static void robtk_pbtn_enter_notify(RobWidget *handle) {
	RobTkPBtn * d = (RobTkPBtn *)GET_HANDLE(handle);
	if (!d->prelight) {
		d->prelight = TRUE;
		queue_draw(d->rw);
	}
}

static void robtk_pbtn_leave_notify(RobWidget *handle) {
	RobTkPBtn * d = (RobTkPBtn *)GET_HANDLE(handle);
	if (d->prelight) {
		d->prelight = FALSE;
		queue_draw(d->rw);
	}
}

static void create_pbtn_pattern(RobTkPBtn * d) {
	d->btn_inactive = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);
	cairo_pattern_add_color_stop_rgb (d->btn_inactive, 0.0, .65, .65, .66);
	cairo_pattern_add_color_stop_rgb (d->btn_inactive, 1.0, .25, .25, .3);

	d->btn_active = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);
	cairo_pattern_add_color_stop_rgb (d->btn_active, 0.0, .3, .3, .33);
	cairo_pattern_add_color_stop_rgb (d->btn_active, 1.0, .8, .8, .82);
}

static void create_pbtn_text_surface(RobTkPBtn * d, const char * txt, PangoFontDescription *font) {
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
	get_color_from_theme(0, c_col);
	write_text_full(cr, txt, font,
			d->w_width / 2.0 + 1,
			d->w_height / 2.0 + 1, 0, 2, c_col);
	cairo_destroy (cr);
}

/******************************************************************************
 * RobWidget stuff
 */

static void
priv_pbtn_size_request(RobWidget* handle, int *w, int *h) {
	RobTkPBtn * d = (RobTkPBtn *)GET_HANDLE(handle);
	*w = d->w_width;
	*h = d->w_height;
}


/******************************************************************************
 * public functions
 */

static RobTkPBtn * robtk_pbtn_new(const char * txt) {
	assert(txt);
	RobTkPBtn *d = (RobTkPBtn *) malloc(sizeof(RobTkPBtn));

	d->cb = NULL;
	d->handle = NULL;
	d->cb_up = NULL;
	d->handle_up = NULL;
	d->cb_down = NULL;
	d->handle_down = NULL;
	d->sf_txt = NULL;
	d->sensitive = TRUE;
	d->prelight = FALSE;
	d->enabled = FALSE;

	int ww, wh;
	PangoFontDescription *fd = get_font_from_theme();

	get_text_geometry(txt, fd, &ww, &wh);
	d->w_width = ww + 14;
	d->w_height = wh + 8;
	d->l_width = d->w_width;

	create_pbtn_text_surface(d, txt, fd);
	pango_font_description_free(fd);

	d->rw = robwidget_new(d);
	ROBWIDGET_SETNAME(d->rw, "pbtn");
	robwidget_set_alignment(d->rw, 0, .5);

	robwidget_set_size_request(d->rw, priv_pbtn_size_request);
	robwidget_set_expose_event(d->rw, robtk_pbtn_expose_event);
	robwidget_set_mouseup(d->rw, robtk_pbtn_mouseup);
	robwidget_set_mousedown(d->rw, robtk_pbtn_mousedown);
	robwidget_set_enter_notify(d->rw, robtk_pbtn_enter_notify);
	robwidget_set_leave_notify(d->rw, robtk_pbtn_leave_notify);

	create_pbtn_pattern(d);
	return d;
}

static void robtk_pbtn_destroy(RobTkPBtn *d) {
	robwidget_destroy(d->rw);
	cairo_pattern_destroy(d->btn_active);
	cairo_pattern_destroy(d->btn_inactive);
	cairo_surface_destroy(d->sf_txt);
	free(d);
}

static void robtk_pbtn_set_alignment(RobTkPBtn *d, float x, float y) {
	robwidget_set_alignment(d->rw, x, y);
}

static RobWidget * robtk_pbtn_widget(RobTkPBtn *d) {
	return d->rw;
}

static void robtk_pbtn_set_callback(RobTkPBtn *d, bool (*cb) (RobWidget* w, void* handle), void* handle) {
	d->cb = cb;
	d->handle = handle;
}

static void robtk_pbtn_set_callback_up(RobTkPBtn *d, bool (*cb) (RobWidget* w, void* handle), void* handle) {
	d->cb_up = cb;
	d->handle_up = handle;
}

static void robtk_pbtn_set_callback_down(RobTkPBtn *d, bool (*cb) (RobWidget* w, void* handle), void* handle) {
	d->cb_down = cb;
	d->handle_down = handle;
}

static void robtk_pbtn_set_sensitive(RobTkPBtn *d, bool s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		queue_draw(d->rw);
	}
}

static bool robtk_pbtn_get_pushed(RobTkPBtn *d) {
	return (d->enabled);
}
#endif
