/* combobox-like widget - select one of N texts
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

#ifndef _ROB_TK_SELECTOR_H_
#define _ROB_TK_SELECTOR_H_

#include "robtk_label.h"

struct select_item {
	RobTkLbl* lbl;
	float value;
	int width;
};

typedef struct {
	RobWidget* rw;
	struct select_item *items;

	bool sensitive;
	bool prelight;
	int  lightarr;

	bool wraparound;

	bool (*cb) (RobWidget* w, gpointer handle);
	gpointer handle;
	int active_item;
	int item_count;
	int dfl;

	pthread_mutex_t _mutex;
	float w_width, w_height;
	float t_width, t_height;
} RobTkSelect;

/******************************************************************************
 * child callbacks
 */

static bool robtk_select_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t* ev) {
	RobTkSelect * d = (RobTkSelect *)GET_HANDLE(handle);
	assert(d->items != NULL);
	assert(d->active_item < d->item_count);

	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
	cairo_clip(cr);

	/* background */
	float cbg[4];
	// TODO get these colors from the theme..
	float cbb[4] = {.2, .2, .2, 1.0 };
	float cbp[4] = {.3, .3, .3, 1.0 };
	float cfg[4] = {.4, .4, .4, 1.0 };

	get_color_from_theme(1, cbg);
	cairo_set_source_rgb (cr, cbg[0], cbg[1], cbg[2]);

	rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
	cairo_fill(cr);

	/* draw arrows left + right */
	int w_width = d->w_width;
	const int w_h2 = d->w_height / 2;

	cairo_set_line_width(cr, 1.0);

	if (!d->sensitive || (!d->wraparound && d->active_item == 0)) {
		cairo_set_source_rgba(cr, cbg[0], cbg[1], cbg[2], 1.0);
	} else if (d->sensitive && d->prelight && d->lightarr == -1) {
		cairo_set_source_rgba(cr, cbp[0], cbp[1], cbp[2], 1.0);
	} else {
		cairo_set_source_rgba(cr, cbb[0], cbb[1], cbb[2], 1.0);
	}
	cairo_rectangle(cr, 2.5, 2.5, 14, d->w_height - 4);
	cairo_fill(cr);

	cairo_set_source_rgba(cr, cfg[0], cfg[1], cfg[2], 1.0);
	cairo_set_source_rgba(cr, .4, .4, .4, 1.0);
	cairo_move_to(cr, 12, w_h2 - 3.5);
	cairo_line_to(cr,  8, w_h2 + 0.5);
	cairo_line_to(cr, 12, w_h2 + 4.5);
	cairo_stroke(cr);

	if (!d->sensitive || (!d->wraparound && d->active_item == d->item_count -1)) {
		cairo_set_source_rgba(cr, cbg[0], cbg[1], cbg[2], 1.0);
	} else if (d->prelight && d->lightarr == 1) {
		cairo_set_source_rgba(cr, cbp[0], cbp[1], cbp[2], 1.0);
	} else {
		cairo_set_source_rgba(cr, cbb[0], cbb[1], cbb[2], 1.0);
	}
	cairo_rectangle(cr, w_width - 15.5, 2.5, 14, d->w_height - 4);
	cairo_fill(cr);

	cairo_set_source_rgba(cr, cfg[0], cfg[1], cfg[2], 1.0);
	cairo_move_to(cr, w_width - 10.5, w_h2 - 3.5);
	cairo_line_to(cr, w_width -  6.5, w_h2 + 0.5);
	cairo_line_to(cr, w_width - 10.5, w_h2 + 4.5);
	cairo_stroke(cr);

	cairo_save(cr);
	const float off = floor(16 + (d->t_width - d->items[d->active_item].width) / 2.0);
	cairo_translate(cr, off, 3);
	cairo_rectangle_t a;
	a.x=0; a.width = d->items[d->active_item].width;
	a.y=0; a.height = d->t_height;
	robtk_lbl_expose_event(d->items[d->active_item].lbl->rw, cr, &a);
	cairo_restore(cr);

	rounded_rectangle(cr, 2.5, 2.5, d->w_width - 4, d->w_height -4, 6);
	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgba(cr, .0, .0, .0, 1.0);
	cairo_stroke(cr);

	if (!d->sensitive) {
		cairo_set_source_rgba(cr, .3, .3, .3, 0.5);
		cairo_rectangle(cr, 0, 0, w_width, d->w_height);
		cairo_fill(cr);
	}

	return TRUE;
}

static void robtk_select_set_active_item(RobTkSelect *d, int i) {
	if (i<0 || i >= d->item_count) return;
	if (i == d->active_item) return;
	d->active_item = i;
	if (d->cb) d->cb(d->rw, d->handle);
	queue_draw(d->rw);
}

static RobWidget* robtk_select_mouseup(RobWidget* handle, RobTkBtnEvent *ev) {
	RobTkSelect * d = (RobTkSelect *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }

	if (ev->state & ROBTK_MOD_SHIFT) {
		robtk_select_set_active_item(d, d->dfl);
		return NULL;
	}

	int active_item = d->active_item;
	if (ev->x <= 18) {
		if (d->wraparound) {
			active_item = (d->active_item + d->item_count - 1) % d->item_count;
		} else {
				active_item--;
		}
	} else if (ev->x >= d->w_width - 18) {
		if (d->wraparound) {
			active_item = (d->active_item + 1) % d->item_count;
		} else {
			active_item++;
		}
	}

	robtk_select_set_active_item(d, active_item);
	return NULL;
}

static RobWidget* robtk_select_mousemove(RobWidget* handle, RobTkBtnEvent *ev) {
	RobTkSelect * d = (RobTkSelect *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }
	int pla = 0;
	if (ev->x <= 18) {
		if (d->wraparound || d->active_item != 0) pla = -1;
	} else if (ev->x >= d->w_width - 18) {
		if (d->wraparound || d->active_item != d->item_count -1) pla = 1;
	}
	if (pla != d->lightarr) {
		d->lightarr = pla;
		queue_draw(d->rw);
	}
	return NULL;
}

static RobWidget* robtk_select_scroll(RobWidget* handle, RobTkBtnEvent *ev) {
	RobTkSelect * d = (RobTkSelect *)GET_HANDLE(handle);
	if (!d->sensitive) { return NULL; }

	int active_item = d->active_item;
	switch (ev->direction) {
		case ROBTK_SCROLL_RIGHT:
		case ROBTK_SCROLL_UP:
			if (d->wraparound) {
				active_item = (d->active_item + 1) % d->item_count;
			} else {
				active_item++;
			}
			break;
		case ROBTK_SCROLL_LEFT:
		case ROBTK_SCROLL_DOWN:
			if (d->wraparound) {
				active_item = (d->active_item + d->item_count - 1) % d->item_count;
			} else {
				active_item--;
			}
			break;
		default:
			break;
	}
	robtk_select_set_active_item(d, active_item);
	return handle;
}

static void robtk_select_enter_notify(RobWidget *handle) {
	RobTkSelect * d = (RobTkSelect *)GET_HANDLE(handle);
	if (!d->prelight) {
		d->prelight = TRUE;
		queue_draw(d->rw);
	}
}

static void robtk_select_leave_notify(RobWidget *handle) {
	RobTkSelect * d = (RobTkSelect *)GET_HANDLE(handle);
	if (d->prelight) {
		d->prelight = FALSE;
		queue_draw(d->rw);
	}
}


static void
robtk_select_size_request(RobWidget* handle, int *w, int *h) {
	RobTkSelect * d = (RobTkSelect *)GET_HANDLE(handle);
	d->w_width = 36 + d->t_width;
	d->w_height = MAX(16, 6 + d->t_height);
	*w = d->w_width;
	*h = d->w_height;
}



/******************************************************************************
 * public functions
 */

static RobTkSelect * robtk_select_new() {
	RobTkSelect *d = (RobTkSelect *) malloc(sizeof(RobTkSelect));

	d->sensitive = TRUE;
	d->prelight = FALSE;
	d->lightarr = 0;
	d->cb = NULL;
	d->handle = NULL;
	pthread_mutex_init (&d->_mutex, 0);

	d->wraparound = FALSE;
	d->items = NULL;
	d->item_count = d->active_item = d->dfl = 0;
	d->w_width = d->w_height = 0;
	d->t_width = d->t_height = 0;

	d->rw = robwidget_new(d);
	ROBWIDGET_SETNAME(d->rw, "select");
	robwidget_set_expose_event(d->rw, robtk_select_expose_event);
	robwidget_set_size_request(d->rw, robtk_select_size_request);
	robwidget_set_mouseup(d->rw, robtk_select_mouseup);
	robwidget_set_mousemove(d->rw, robtk_select_mousemove);
	robwidget_set_mousescroll(d->rw, robtk_select_scroll);
	robwidget_set_enter_notify(d->rw, robtk_select_enter_notify);
	robwidget_set_leave_notify(d->rw, robtk_select_leave_notify);
	return d;
}

static void robtk_select_destroy(RobTkSelect *d) {
	int i;
	for (i=0; i < d->item_count ; ++i) {
		robtk_lbl_destroy(d->items[i].lbl);
	}
	robwidget_destroy(d->rw);
	free(d->items);
	pthread_mutex_destroy(&d->_mutex);

	free(d);
}

static void robtk_select_set_alignment(RobTkSelect *d, float x, float y) {
	robwidget_set_alignment(d->rw, x, y);
}

static void robtk_select_add_item(RobTkSelect *d, float val, const char *txt) {
	d->items = (struct select_item *) realloc(d->items, sizeof(struct select_item) * (d->item_count + 1));
	d->items[d->item_count].value = val;
	d->items[d->item_count].lbl = robtk_lbl_new(txt);
	int w, h;
	priv_lbl_size_request(d->items[d->item_count].lbl->rw, &w, &h);
	d->t_width = MAX(d->t_width, w);
	d->t_height = MAX(d->t_height, h);
	d->items[d->item_count].width = w;
	d->item_count++;
}

static RobWidget * robtk_select_widget(RobTkSelect *d) {
	return d->rw;
}

static void robtk_select_set_callback(RobTkSelect *d, bool (*cb) (RobWidget* w, gpointer handle), gpointer handle) {
	d->cb = cb;
	d->handle = handle;
}

static void robtk_select_set_default_item(RobTkSelect *d, int i) {
	d->dfl = i;
}

static void robtk_select_set_item(RobTkSelect *d, int i) {
	robtk_select_set_active_item(d, i);
}

static void robtk_select_set_value(RobTkSelect *d, float v) {
	assert(d->item_count > 0);
	int i;
	int s = 0;
	float diff = fabsf(v - d->items[0].value);
	for (i=1; i < d->item_count; ++i) {
		float df = fabsf(v - d->items[i].value);
		if (df < diff) {
			s = i;
			diff = df;
		}
	}
	robtk_select_set_active_item(d, s);
}


static void robtk_select_set_sensitive(RobTkSelect *d, bool s) {
	if (d->sensitive != s) {
		d->sensitive = s;
	}
	queue_draw(d->rw);
}

static int robtk_select_get_item(RobTkSelect *d) {
	return d->active_item;
}

static float robtk_select_get_value(RobTkSelect *d) {
	return d->items[d->active_item].value;
}
#endif
