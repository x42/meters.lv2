/* radio button widget
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

#ifndef _ROB_TK_CBTN_H_
#define _ROB_TK_CBTN_H_

#define GBT_LED_RADIUS (11.0)
enum GedLedMode {
	GBT_LED_RADIO  = -2,
	GBT_LED_LEFT  = -1,
	GBT_LED_OFF   = 0,
	GBT_LED_RIGHT = 1
};

typedef struct {
	RobWidget* rw;

	bool sensitive;
	bool prelight;
	bool enabled;

	enum GedLedMode show_led;
	bool flat_button;
	bool radiomode;

	bool (*cb) (RobWidget* w, void* handle);
	void* handle;

	cairo_pattern_t* btn_enabled;
	cairo_pattern_t* btn_inactive;
	cairo_pattern_t* btn_led;
	cairo_surface_t* sf_txt_normal;
	cairo_surface_t* sf_txt_enabled;

	float w_width, w_height, l_width;
	float c_on[4];
	float coff[4];
} RobTkCBtn;

/******************************************************************************
 * some helpers
 */

static void robtk_cbtn_update_enabled(RobTkCBtn * d, bool enabled) {
	if (enabled != d->enabled) {
		d->enabled = enabled;
		if (d->cb) d->cb(d->rw, d->handle);
		queue_draw(d->rw);
	}
}

static void create_cbtn_pattern(RobTkCBtn * d) {
	d->btn_inactive = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);
	cairo_pattern_add_color_stop_rgb (d->btn_inactive, 0.0, .65, .65, .66);
	cairo_pattern_add_color_stop_rgb (d->btn_inactive, 1.0, .25, .25, .3);

	d->btn_enabled = cairo_pattern_create_linear (0.0, 0.0, 0.0, d->w_height);
	if (d->show_led == GBT_LED_OFF) {
		cairo_pattern_add_color_stop_rgb (d->btn_enabled, 0.0, .2, .5, .21);
		cairo_pattern_add_color_stop_rgb (d->btn_enabled, 1.0, .5, .9, .51);
	} else {
		cairo_pattern_add_color_stop_rgb (d->btn_enabled, 0.0, .3, .3, .33);
		cairo_pattern_add_color_stop_rgb (d->btn_enabled, 1.0, .8, .8, .82);
	}

	d->btn_led = cairo_pattern_create_linear (0.0, 0.0, 0.0, GBT_LED_RADIUS);
	cairo_pattern_add_color_stop_rgba (d->btn_led, 0.0, 0.0, 0.0, 0.0, 0.4);
	cairo_pattern_add_color_stop_rgba (d->btn_led, 1.0, 1.0, 1.0, 1.0 , 0.7);
}

static void create_cbtn_text_surface(RobTkCBtn * d, const char * txt, PangoFontDescription *font) {
	float c_col[4];
	get_color_from_theme(0, c_col);

	create_text_surface(&d->sf_txt_normal,
			d->w_width, d->w_height,
			1 +
			(d->w_width - (d->show_led ? GBT_LED_RADIUS + 6 : 0)) / 2.0
			 + (d->show_led < 0 ? GBT_LED_RADIUS + 6 : 0),
			d->w_height / 2.0 + 1,
			txt, font, c_col);

	get_color_from_theme(2, c_col);

	create_text_surface(&d->sf_txt_enabled,
			d->w_width, d->w_height,
			1 +
			(d->w_width - (d->show_led ? GBT_LED_RADIUS + 6 : 0)) / 2.0
			 + (d->show_led < 0 ? GBT_LED_RADIUS + 6 : 0),
			d->w_height / 2.0 + 1,
			txt, font, c_col);
}


/******************************************************************************
 * main drawing callback
 */

static bool robtk_cbtn_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t* ev) {
	RobTkCBtn * d = (RobTkCBtn *)GET_HANDLE(handle);
	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);
	float led_r, led_g, led_b; // TODO consolidate with c[]

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
			led_r = c[0];
			led_g = c[1];
			led_b = c[2];
		} else if (d->enabled) {
			if (d->radiomode) {
				led_r = .3; led_g = .8; led_b = .1;
			} else {
				led_r = d->c_on[0]; led_g = d->c_on[1]; led_b = d->c_on[2];
			}
		} else {
			if (d->radiomode) {
				led_r = .1; led_g = .3; led_b = .1;
			} else {
				led_r = d->coff[0]; led_g = d->coff[1]; led_b = d->coff[2];
			}
		}


	if (!d->flat_button) {
		if (d->enabled) {
			cairo_set_source(cr, d->btn_enabled);
		} else if (!d->sensitive) {
			cairo_set_source_rgb (cr, c[0], c[1], c[2]);
		} else {
			cairo_set_source(cr, d->btn_inactive);
		}

		rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
		cairo_fill_preserve (cr);
		if (!d->sensitive && d->enabled) {
			cairo_set_source_rgba (cr, c[0], c[1], c[2], .6);
			cairo_fill_preserve (cr);
		}
		cairo_set_line_width (cr, .75);
		cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
		cairo_stroke(cr);
	}

	if (d->flat_button && !d->sensitive) {
		//cairo_set_operator (cr, CAIRO_OPERATOR_XOR); // check
		cairo_set_operator (cr, CAIRO_OPERATOR_EXCLUSION);
		cairo_set_source_surface(cr, d->sf_txt_normal, rint((d->w_width - d->l_width) / 2.0), 0);
	} else if (!d->flat_button && d->enabled) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface(cr, d->sf_txt_enabled, rint((d->w_width - d->l_width) / 2.0), 0);
	} else {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface(cr, d->sf_txt_normal, rint((d->w_width - d->l_width) / 2.0), 0);
	}
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
			cairo_fill(cr);
		} else {
			rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
			cairo_fill_preserve(cr);
			cairo_set_line_width (cr, .75);
			cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
			cairo_stroke(cr);
		}
	}
	return TRUE;
}


/******************************************************************************
 * UI callbacks
 */

static RobWidget* robtk_cbtn_mouseup(RobWidget *handle, RobTkBtnEvent *event) {
	RobTkCBtn * d = (RobTkCBtn *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }
	if (!d->prelight) { return NULL; }
	if (d->radiomode && d->enabled) { return NULL; }
	robtk_cbtn_update_enabled(d, ! d->enabled);
	return NULL;
}

static void robtk_cbtn_enter_notify(RobWidget *handle) {
	RobTkCBtn * d = (RobTkCBtn *)GET_HANDLE(handle);
	if (!d->prelight) {
		d->prelight = TRUE;
		queue_draw(d->rw);
	}
}

static void robtk_cbtn_leave_notify(RobWidget *handle) {
	RobTkCBtn * d = (RobTkCBtn *)GET_HANDLE(handle);
	if (d->prelight) {
		d->prelight = FALSE;
		queue_draw(d->rw);
	}
}

/******************************************************************************
 * RobWidget stuff
 */

static void
priv_cbtn_size_request(RobWidget* handle, int *w, int *h) {
	RobTkCBtn* d = (RobTkCBtn*)GET_HANDLE(handle);
	*w = d->w_width;
	*h = d->w_height;
}


/******************************************************************************
 * public functions
 */

static RobTkCBtn * robtk_cbtn_new(const char * txt, enum GedLedMode led, bool flat) {
	assert(txt);
	RobTkCBtn *d = (RobTkCBtn *) malloc(sizeof(RobTkCBtn));

	d->flat_button = flat;
	d->show_led = led;
	d->cb = NULL;
	d->handle = NULL;
	d->sf_txt_normal = NULL;
	d->sf_txt_enabled = NULL;
	d->sensitive = TRUE;
	d->radiomode = FALSE;
	d->prelight = FALSE;
	d->enabled = FALSE;

	d->c_on[0] = .8; d->c_on[1] = .3; d->c_on[2] = .1; d->c_on[3] = 1.0;
	d->coff[0] = .3; d->coff[1] = .1; d->coff[2] = .1; d->coff[3] = 1.0;

	if (led == GBT_LED_RADIO) {
		d->radiomode = TRUE;
	}

	int ww, wh;
	PangoFontDescription *fd = get_font_from_theme();

	get_text_geometry(txt, fd, &ww, &wh);
	assert(d->show_led || ww > 0);
	d->w_width = ((ww > 0) ? (ww + 14) : 7) + (d->show_led ? GBT_LED_RADIUS + 6 : 0);
	d->w_height = wh + 8;
	d->l_width = d->w_width;

	create_cbtn_text_surface(d, txt, fd);
	pango_font_description_free(fd);

	d->rw = robwidget_new(d);
	robwidget_set_alignment(d->rw, 0, .5);
	ROBWIDGET_SETNAME(d->rw, "cbtn");

	robwidget_set_size_request(d->rw, priv_cbtn_size_request);
	robwidget_set_expose_event(d->rw, robtk_cbtn_expose_event);
	robwidget_set_mouseup(d->rw, robtk_cbtn_mouseup);
	robwidget_set_enter_notify(d->rw, robtk_cbtn_enter_notify);
	robwidget_set_leave_notify(d->rw, robtk_cbtn_leave_notify);

	create_cbtn_pattern(d);

	return d;
}

static void robtk_cbtn_destroy(RobTkCBtn *d) {
	robwidget_destroy(d->rw);
	cairo_pattern_destroy(d->btn_enabled);
	cairo_pattern_destroy(d->btn_inactive);
	cairo_pattern_destroy(d->btn_led);
	cairo_surface_destroy(d->sf_txt_normal);
	cairo_surface_destroy(d->sf_txt_enabled);
	free(d);
}

static void robtk_cbtn_set_alignment(RobTkCBtn *d, float x, float y) {
	robwidget_set_alignment(d->rw, x, y);
}

static RobWidget * robtk_cbtn_widget(RobTkCBtn *d) {
	return d->rw;
}

static void robtk_cbtn_set_callback(RobTkCBtn *d, bool (*cb) (RobWidget* w, void* handle), void* handle) {
	d->cb = cb;
	d->handle = handle;
}

static void robtk_cbtn_set_active(RobTkCBtn *d, bool v) {
	robtk_cbtn_update_enabled(d, v);
}

static void robtk_cbtn_set_sensitive(RobTkCBtn *d, bool s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		queue_draw(d->rw);
	}
}

static void robtk_cbtn_set_color_on(RobTkCBtn *d, float r, float g, float b) {
	d->c_on[0] = r;
	d->c_on[1] = g;
	d->c_on[2] = b;
}

static void robtk_cbtn_set_color_off(RobTkCBtn *d, float r, float g, float b) {
	d->coff[0] = r;
	d->coff[1] = g;
	d->coff[2] = b;
}

static bool robtk_cbtn_get_active(RobTkCBtn *d) {
	return (d->enabled);
}
#endif
