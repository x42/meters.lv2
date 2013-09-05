/* label widget
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

#ifndef _ROB_TK_LBL_H_
#define _ROB_TK_LBL_H_

typedef struct {
	RobWidget *rw;

	bool sensitive;
	cairo_surface_t* sf_txt;
	float w_width, w_height;
	float min_width;
	float min_height;
	char *txt;
	pthread_mutex_t _mutex;
} RobTkLbl;

static bool robtk_lbl_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t* ev) {
	RobTkLbl* d = (RobTkLbl *)GET_HANDLE(handle);

	if (pthread_mutex_trylock (&d->_mutex)) {
		queue_draw(d->rw);
		return TRUE;
	}

	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);
	float c[4];
	get_color_from_theme(1, c);
	cairo_set_source_rgb (cr, c[0], c[1], c[2]);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill(cr);

	if (d->sensitive) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	} else {
		cairo_set_operator (cr, CAIRO_OPERATOR_EXCLUSION);
	}
	cairo_set_source_surface(cr, d->sf_txt, 0, 0);
	cairo_paint (cr);

	pthread_mutex_unlock (&d->_mutex);
	return TRUE;
}

static void priv_lbl_prepare_text(RobTkLbl *d, const char *txt) {
	// _mutex must be held to call this function
	int ww, wh;
	float c_col[4];

	PangoFontDescription *fd = get_font_from_theme();
	get_color_from_theme(0, c_col);

	get_text_geometry(txt, fd, &ww, &wh);

	d->w_width = ww + 4;
	d->w_height = wh + 4;

	if (d->w_width < d->min_width) d->w_width = d->min_width;
	if (d->w_height < d->min_height) d->w_height = d->min_height;

	create_text_surface(&d->sf_txt,
			d->w_width, d->w_height,
			d->w_width / 2.0 + 1,
			d->w_height / 2.0 + 1,
			txt, fd, c_col);

	pango_font_description_free(fd);

	robwidget_set_size(d->rw, d->w_width, d->w_height);
	// TODO trigger re-layout  resize_self()

	queue_draw(d->rw);
}

/******************************************************************************
 * RobWidget stuff
 */

static void
priv_lbl_size_request(RobWidget* handle, int *w, int *h) {
	RobTkLbl* d = (RobTkLbl*)GET_HANDLE(handle);
	*w = d->w_width;
	*h = d->w_height;
}

/******************************************************************************
 * public functions
 */

static void robtk_lbl_set_text(RobTkLbl *d, const char *txt) {
	assert(txt);
	pthread_mutex_lock (&d->_mutex);
	free(d->txt);
	d->txt=strdup(txt);
	priv_lbl_prepare_text(d, d->txt);
	pthread_mutex_unlock (&d->_mutex);
}

static RobTkLbl * robtk_lbl_new(const char * txt) {
	assert(txt);
	RobTkLbl *d = (RobTkLbl *) malloc(sizeof(RobTkLbl));

	d->sf_txt = NULL;
	d->min_width = 0;
	d->min_height = 0;
	d->txt = NULL;
	d->sensitive = TRUE;
	pthread_mutex_init (&d->_mutex, 0);
	d->rw = robwidget_new(d);
	ROBWIDGET_SETNAME(d->rw, "label");
	robwidget_set_expose_event(d->rw, robtk_lbl_expose_event);
	robwidget_set_size_request(d->rw, priv_lbl_size_request);

	robtk_lbl_set_text(d, txt);
	return d;
}

static void robtk_lbl_destroy(RobTkLbl *d) {
	robwidget_destroy(d->rw);
	pthread_mutex_destroy(&d->_mutex);
	cairo_surface_destroy(d->sf_txt);
	free(d->txt);
	free(d);
}

static void robtk_lbl_set_alignment(RobTkLbl *d, float x, float y) {
	robwidget_set_alignment(d->rw, x, y);
}

static void robtk_lbl_set_min_geometry(RobTkLbl *d, float w, float h) {
	d->min_width = w;
	d->min_height = h;
	assert(d->txt);
	pthread_mutex_lock (&d->_mutex);
	priv_lbl_prepare_text(d, d->txt);
	pthread_mutex_unlock (&d->_mutex);
}

static RobWidget * robtk_lbl_widget(RobTkLbl *d) {
	return d->rw;
}

static void robtk_lbl_set_sensitive(RobTkLbl *d, bool s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		queue_draw(d->rw);
	}
}
#endif
