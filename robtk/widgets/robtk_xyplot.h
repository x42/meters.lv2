/* XY plot/drawing area
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

#ifndef _ROB_TK_XYP_H_
#define _ROB_TK_XYP_H_

typedef struct {
	RobWidget *rw;
	float w_width, w_height;
	cairo_surface_t* bg;

	float line_width;
	float col[4];

	pthread_mutex_t _mutex;
	uint32_t n_points;
	float *points_x;
	float *points_y;
} RobTkXYp;

static bool robtk_xydraw_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t* ev) {
	RobTkXYp* d = (RobTkXYp *)GET_HANDLE(handle);
	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	if (d->bg) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface(cr, d->bg, 0, 0);
		cairo_paint (cr);
	} else {
		cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
		cairo_set_source_rgba(cr, 0, 0, 0, 1);
		cairo_fill(cr);
	}

	int px = -1;
	float yavg = 0;
	int ycnt = 0;

	pthread_mutex_lock (&d->_mutex);
	for (uint32_t i = 0; i < d->n_points; ++i) {
		float x = d->points_x[i] * d->w_width;
		float y = (1.0 - d->points_y[i]) * d->w_height;

		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x > d->w_width) x = d->w_width;
		if (y > d->w_height) y = d->w_height;

		if (px == rint(x)) {
			yavg += y;
			ycnt ++;
			continue;
		}
		px = rintf(x);
		x = px + .5;
		if (ycnt > 0) {
			y = (yavg + y) / (float)(ycnt+1.0);
		}
		yavg = 0; ycnt = 0;

		if (i==0) cairo_move_to(cr, x, y);
		else cairo_line_to(cr, x, y);
	}
	pthread_mutex_unlock (&d->_mutex);
	if (d->n_points > 0) {
		cairo_set_line_width (cr, d->line_width);
		cairo_set_source_rgba(cr, d->col[0], d->col[1], d->col[2], d->col[3]);
		cairo_stroke(cr);
	}
	return TRUE;
}

/******************************************************************************
 * RobWidget stuff
 */

static void
priv_xydraw_size_request(RobWidget* handle, int *w, int *h) {
	RobTkXYp* d = (RobTkXYp*)GET_HANDLE(handle);
	*w = d->w_width;
	*h = d->w_height;
}


/******************************************************************************
 * public functions
 */

static RobTkXYp * robtk_xydraw_new(int w, int h) {
	RobTkXYp *d = (RobTkXYp *) malloc(sizeof(RobTkXYp));
	d->w_width = w;
	d->w_height = h;
	d->line_width = 1.0;

	d->bg = NULL;
	d->n_points = 0;
	d->points_x = NULL;
	d->points_y = NULL;

	d->col[0] =  .9;
	d->col[1] =  .3;
	d->col[2] =  .2;
	d->col[3] = 1.0;

	pthread_mutex_init (&d->_mutex, 0);
	d->rw = robwidget_new(d);
	ROBWIDGET_SETNAME(d->rw, "xydraw");
	robwidget_set_expose_event(d->rw, robtk_xydraw_expose_event);
	robwidget_set_size_request(d->rw, priv_xydraw_size_request);

	return d;
}

static void robtk_xydraw_destroy(RobTkXYp *d) {
	pthread_mutex_destroy(&d->_mutex);
	robwidget_destroy(d->rw);
	d->n_points = 0;
	free(d->points_x);
	free(d->points_y);
	free(d);
}

static void robtk_xydraw_set_alignment(RobTkXYp *d, float x, float y) {
	robwidget_set_alignment(d->rw, x, y);
}

static void robtk_xydraw_set_linewidth(RobTkXYp *d, float lw) {
	d->line_width = lw;
}

static void robtk_xydraw_set_color(RobTkXYp *d, float r, float g, float b, float a) {
	d->col[0] = r;
	d->col[1] = g;
	d->col[2] = b;
	d->col[3] = a;
}

static void robtk_xydraw_set_points(RobTkXYp *d, uint32_t np, float *xp, float *yp) {
	pthread_mutex_lock (&d->_mutex);
	d->points_x = (float*) realloc(d->points_x, sizeof(float) * np);
	d->points_y = (float*) realloc(d->points_y, sizeof(float) * np);
	memcpy(d->points_x, xp, sizeof(float) * np);
	memcpy(d->points_y, yp, sizeof(float) * np);
	d->n_points = np;
	pthread_mutex_unlock (&d->_mutex);
	queue_draw(d->rw);
}

static void robtk_xydraw_set_surface(RobTkXYp *d, cairo_surface_t *s) {
	d->bg = s;
}

static RobWidget * robtk_xydraw_widget(RobTkXYp *d) {
	return d->rw;
}
#endif
