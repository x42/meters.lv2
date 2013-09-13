/* robwidget - widget layout packing
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

/*****************************************************************************/
/* common container functions */

//#define DEBUG_HBOX
//#define DEBUG_VBOX
//#define DEBUG_TABLE

struct rob_container {
	bool homogeneous;
	bool expand;
	int padding;
};

struct rob_table_child {
	RobWidget *rw;
	int left;
	int right;
	int top;
	int bottom;
	int xpadding;
	int ypadding;
};

struct rob_table_field {
	int req_w;
	int req_h;
	bool is_expandable_x;
	bool is_expandable_y;
	int acq_w;
	int acq_h;
#if 0
	int off_x;
	int off_y;
#endif
};

struct rob_table {
	bool homogeneous;
	bool expand;
	unsigned int nrows;
	unsigned int ncols;
	unsigned int nchilds;
	struct rob_table_child *chld;
	struct rob_table_field *rows;
	struct rob_table_field *cols;
};

static void rob_table_resize(struct rob_table *rt, unsigned int nrows, unsigned int ncols) {
	if (rt->ncols >= ncols && rt->nrows >= nrows) return;
#ifdef DEBUG_TABLE
	printf("rob_table_resize %d %d\n", nrows, ncols);
#endif

	if (rt->nrows != nrows) {
		rt->rows = (struct rob_table_field*) realloc(rt->rows, sizeof(struct rob_table_field) * nrows);
		rt->nrows = nrows;
	}
	if (rt->ncols != ncols) {
		rt->cols = (struct rob_table_field*) realloc(rt->cols, sizeof(struct rob_table_field) * ncols);
		rt->ncols = ncols;
	}
}

static void robwidget_position_cache(RobWidget *rw) {
	RobTkBtnEvent e;
	e.x = 0; e.y = 0;
	offset_traverse_from_child(rw, &e);

	rw->trel.x = e.x; rw->trel.y = e.y;
	rw->trel.width  = rw->area.width;
	rw->trel.height = rw->area.height;
	rw->resized = TRUE;
}

static void robwidget_position_set(RobWidget *rw,
		const int pw, const int ph) {
	rw->area.x = rint((pw - rw->area.width) * rw->xalign);
	rw->area.y = rint((ph - rw->area.height) * rw->yalign);
}

static void rtable_size_allocate(RobWidget* rw, int w, int h);
static void rhbox_size_allocate(RobWidget* rw, int w, int h);
static void rvbox_size_allocate(RobWidget* rw, int w, int h);

static bool roblayout_can_expand(RobWidget *rw) {
	bool can_expand = FALSE;
	if (   rw->size_allocate == rhbox_size_allocate
	    || rw->size_allocate == rvbox_size_allocate) {
		can_expand = ((struct rob_container*)rw->self)->expand;
	} else if (rw->size_allocate == rtable_size_allocate) {
		can_expand = ((struct rob_table*)rw->self)->expand;
	} else if (rw->size_allocate) {
		can_expand = TRUE;
	}
	return can_expand;
}

static void rcontainer_child_pack(RobWidget *rw, RobWidget *chld, bool expand) {
#ifndef NDEBUG
	if (chld->parent) {
		fprintf(stderr, "re-parent child\n");
	}
#endif
	if (   chld->size_allocate == rhbox_size_allocate
	    || chld->size_allocate == rvbox_size_allocate
			) {
		((struct rob_container*)chld->self)->expand = expand;
	}
#if 0 // TODO fix table no-expansion calc.
	if (chld->size_allocate == rtable_size_allocate) {
		((struct rob_table*)chld->self)->expand = expand;
	}
#endif
	rw->children = (RobWidget**) realloc(rw->children, (rw->childcount + 1) * sizeof(RobWidget *));
	rw->children[rw->childcount] = chld;
	rw->childcount++;
	chld->parent = rw;
}
static void rcontainer_clear_bg(RobWidget* rw, cairo_t* cr, cairo_rectangle_t *ev) {
  cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
  cairo_clip (cr);

  float c[4];
#ifdef VISIBLE_EXPOSE
	c[0] = rand() / (float)RAND_MAX;
	c[1] = rand() / (float)RAND_MAX;
	c[2] = rand() / (float)RAND_MAX;
	c[3] = 1.0;
#else
  get_color_from_theme(1, c);
#endif
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgb (cr, c[0], c[1], c[2]);
  cairo_rectangle (cr, 0, 0, ev->width, ev->height);
  cairo_fill(cr);
}

/*****************************************************************************/
/* common container functions, for event propagation */

#define MOUSEEVENT \
	RobTkBtnEvent event; \
	event.x = ev->x - c->area.x; \
	event.y = ev->y - c->area.y; \
	event.state = ev->state; \
	event.direction = ev->direction;

static RobWidget* rcontainer_mousedown(RobWidget* handle, RobTkBtnEvent *ev) {
	RobWidget * rw = (RobWidget*)handle;
	RobWidget * c = robwidget_child_at(rw, ev->x, ev->y);
	if (!c || !c->mousedown) return NULL;
	if (c->hidden) return NULL;
	MOUSEEVENT
	return c->mousedown(c, &event);
}

static RobWidget* rcontainer_mouseup(RobWidget* handle, RobTkBtnEvent *ev) {
	RobWidget * rw = (RobWidget*)handle;
	RobWidget * c = robwidget_child_at(rw, ev->x, ev->y);
	if (!c || !c->mouseup) return NULL;
	if (c->hidden) return NULL;
	MOUSEEVENT
	return c->mouseup(c, &event);
}

static RobWidget* rcontainer_mousemove(RobWidget* handle, RobTkBtnEvent *ev) {
	RobWidget * rw = (RobWidget*)handle;
	RobWidget * c = robwidget_child_at(rw, ev->x, ev->y);
	if (!c || !c->mousemove) return NULL;
	if (c->hidden) return NULL;
	MOUSEEVENT
	return c->mousemove(c, &event);
}

static RobWidget* rcontainer_mousescroll(RobWidget* handle, RobTkBtnEvent *ev) {
	RobWidget * rw = (RobWidget*)handle;
	RobWidget * c = robwidget_child_at(rw, ev->x, ev->y);
	if (!c || !c->mousescroll) return NULL;
	if (c->hidden) return NULL;
	MOUSEEVENT
	return c->mousescroll(c, &event);
}

static bool rcontainer_expose_event(RobWidget* rw, cairo_t* cr, cairo_rectangle_t *ev) {

#if 1 // box background
	if (rw->resized) {
		cairo_rectangle_t event;
		event.x = MAX(0, ev->x - rw->area.x);
		event.y = MAX(0, ev->y - rw->area.y);
		event.width  = MIN(rw->area.x + rw->area.width , ev->x + ev->width)   - MAX(ev->x, rw->area.x);
		event.height = MIN(rw->area.y + rw->area.height, ev->y + ev->height) - MAX(ev->y, rw->area.y);
		cairo_save(cr);
		rcontainer_clear_bg(rw, cr, &event);
		cairo_restore(cr);
	}
#endif

	for (unsigned int i=0; i < rw->childcount; ++i) {
		RobWidget * c = (RobWidget *) rw->children[i];
		cairo_rectangle_t event;
		if (c->hidden) continue;
		if (!rect_intersect(&c->area, ev)) continue;

		if (rw->resized) {
			// XXX alternatively set  c->resized = TRUE
			memcpy(&event, ev, sizeof(cairo_rectangle_t));
		} else {
			event.x = MAX(0, ev->x - c->area.x);
			event.y = MAX(0, ev->y - c->area.y);
			event.width  = MIN(c->area.x + c->area.width, ev->x + ev->width) - MAX(ev->x,  c->area.x);
			event.height = MIN(c->area.y + c->area.height, ev->y + ev->height) - MAX(ev->y, c->area.y);
		}
#ifdef DEBUG_EXPOSURE_UI
		printf("rce %.1f+%.1f , %.1fx%.1f  ||  cld %.1f+%.1f ,  %.1fx%.1f || ISC %.1f+%.1f , %.1fx%.1f\n",
				ev->x, ev->y, ev->width, ev->height,
				c->area.x, c->area.y,  c->area.width, c->area.height,
				event.x, event.y, event.width, event.height);
#endif
		cairo_save(cr);
		cairo_translate(cr, c->area.x, c->area.y);
		c->expose_event(c, cr, &event);
		cairo_restore(cr);
	}
	if (rw->resized) {
		rw->resized = FALSE;
	}
	return TRUE;
}

/*****************************************************************************/
/* specific containers */

/* horizontal box */
static void
rhbox_size_request(RobWidget* rw, int *w, int *h) {
	assert(w && h);
	int ww = 0;
	int hh = 0;
	bool homogeneous = ((struct rob_container*)rw->self)->homogeneous;
	int padding = ((struct rob_container*)rw->self)->padding;

	int cnt = 0;
	for (unsigned int i=0; i < rw->childcount; ++i) {
		int cw, ch;
		RobWidget * c = (RobWidget *) rw->children[i];
		if (c->hidden) continue;
		c->size_request(c, &cw, &ch);
		if (homogeneous) {
			ww = MAX(cw, ww);
		} else {
			ww += cw;
		}
		hh = MAX(ch, hh);
		c->area.width = cw;
		c->area.height = ch;
		cnt++;
#ifdef DEBUG_HBOX
		printf("HBOXCHILD %d wants %dx%d (new total: %dx%d)\n", i, cw, ch, ww, hh);
#endif
	}

	if (homogeneous) {
		for (unsigned int i=0; i < rw->childcount; ++i) {
			RobWidget * c = (RobWidget *) rw->children[i];
			if (c->hidden) continue;
			c->area.width = ww;
		}
		ww *= cnt;
#ifdef DEBUG_HBOX
		printf("HBOXCHILD set homogenious x %d (total: %dx%d)\n", cnt, ww, hh);
#endif
	}

	if (cnt > 0) {
		ww += (cnt-1) * padding;
	}

	ww = ceil(ww);
	hh = ceil(hh);
	*w = ww;
	*h = hh;
	robwidget_set_area(rw, 0, 0, ww, hh);
}

static void rhbox_size_allocate(RobWidget* rw, int w, int h) {
#ifdef DEBUG_HBOX
	printf("Hbox size_allocate %d, %d\n", w, h);
#endif
	int padding = ((struct rob_container*)rw->self)->padding;
	bool expand = ((struct rob_container*)rw->self)->expand;
	if (w < rw->area.width) {
		printf(" !!! hbox packing error\n");
		w = rw->area.width;
	}
	float xtra_space = 0;
	bool grow = FALSE;

	if (w > rw->area.width) {
		// check what widgets can expand..
		int cnt = 0;
		int exp = 0;
		for (unsigned int i=0; i < rw->childcount; ++i) {
			RobWidget * c = (RobWidget *) rw->children[i];
			if (c->hidden) continue;
			++cnt;
			if (!roblayout_can_expand(c)) continue;
			if (c->size_allocate) { ++exp;}
		}
		if (exp > 0) {
			/* divide extra space.. */
			xtra_space = (w - rw->area.width /* - (cnt-1) * padding */) / (float)exp;
#ifdef DEBUG_HBOX
			printf("expand %d widgets by %.1f width\n", exp, xtra_space);
#endif
		} else if (!rw->position_set) {
			xtra_space = (w - rw->area.width) / 2.0;
			grow = TRUE;
#ifdef DEBUG_HBOX
			printf("grow self by %.1f width\n", xtra_space);
#endif
		} else {
#ifdef DEBUG_HBOX
			printf("don't grow\n");
#endif
		}
	}

	const int hh = /* expand-other (height in hbox)*/ 0 ? h : rw->area.height;
	/* allocate kids */
	for (unsigned int i=0; i < rw->childcount; ++i) {
		RobWidget * c = (RobWidget *) rw->children[i];
		if (c->hidden) continue;
		if (c->size_allocate) {
			c->size_allocate(c, c->area.width + (grow ? 0 : floorf(xtra_space)), hh);
		}
#ifdef DEBUG_HBOX
		printf("HBOXCHILD %d use %.1fx%.1f\n", i, c->area.width, c->area.height);
#endif
	}

	/* set position after allocation */
	float ww = grow ? xtra_space : 0;
	int ccnt = 0;
	for (unsigned int i=0; i < rw->childcount; ++i) {
		RobWidget * c = (RobWidget *) rw->children[i];
		if (c->hidden) continue;
		if (++ccnt != 1) { ww += padding; }
		if (c->position_set) {
			c->position_set(c, c->area.width, hh);
		} else {
			robwidget_position_set(c, c->area.width, hh);
		}
		c->area.x += floorf(ww);
		c->area.y += 0;
		c->area.y += floor((h - hh) / 2.0);
		ww += c->area.width;

#ifdef DEBUG_HBOX
		printf("HBOXCHILD %d '%s' packed to %.1f+%.1f  %.1fx%.1f\n", i,
				ROBWIDGET_NAME(c),
				c->area.x, c->area.y, c->area.width, c->area.height);
#endif
		if (c->redraw_pending) {
			queue_draw(c);
		}
	}
#ifdef DEBUG_HBOX
	if (grow) ww += xtra_space;
	if (ww != w) {
		printf("MISSED STH :) width: %.1f <> parent-w: %d\n", ww, w);
	}
#endif
	if (expand) {
		ww = w;
	} else {
		ww = rint(ww);
	}
	robwidget_set_area(rw, 0, 0, ww, h);
}

static void rob_hbox_child_pack(RobWidget *rw, RobWidget *chld, bool expand) {
	rcontainer_child_pack(rw, chld, expand);
}

static RobWidget * rob_hbox_new(bool homogeneous, int padding) {
	RobWidget * rw = robwidget_new(NULL);
	ROBWIDGET_SETNAME(rw, "hbox");
	rw->self = (struct rob_container*) calloc(1, sizeof(struct rob_container));
	((struct rob_container*)rw->self)->homogeneous = homogeneous;
	((struct rob_container*)rw->self)->padding = padding;
	((struct rob_container*)rw->self)->expand = TRUE;

	rw->size_request  = rhbox_size_request;
	rw->size_allocate = rhbox_size_allocate;

	rw->expose_event = rcontainer_expose_event;
	rw->mouseup      = rcontainer_mouseup;
	rw->mousedown    = rcontainer_mousedown;
	rw->mousemove    = rcontainer_mousemove;
	rw->mousemove    = rcontainer_mousemove;
	rw->mousescroll  = rcontainer_mousescroll;

	rw->area.x=0;
	rw->area.y=0;
	rw->area.width = 0;
	rw->area.height = 0;

	return rw;
}

/* vertical box */
static void rvbox_size_request(RobWidget* rw, int *w, int *h) {
	assert(w && h);
	int ww = 0;
	int hh = 0;
	bool homogeneous = ((struct rob_container*)rw->self)->homogeneous;
	int padding = ((struct rob_container*)rw->self)->padding;

	int cnt = 0;
	for (unsigned int i=0; i < rw->childcount; ++i) {
		int cw, ch;
		RobWidget * c = (RobWidget *) rw->children[i];
		if (c->hidden) continue;
		c->size_request(c, &cw, &ch);
		ww = MAX(cw, ww);
		if (homogeneous) {
			hh = MAX(ch, hh);
		} else {
			hh += ch;
		}
		c->area.width = cw;
		c->area.height = ch;
		cnt++;
#ifdef DEBUG_VBOX
		printf("VBOXCHILD %d ('%s') wants %dx%d (new total: %dx%d)\n", i,
				ROBWIDGET_NAME(rw->children[i]), cw, ch, ww, hh);
#endif
	}

	if (homogeneous) {
		for (unsigned int i=0; i < rw->childcount; ++i) {
			RobWidget * c = (RobWidget *) rw->children[i];
			if (c->hidden) continue;
			c->area.height = hh;
		}
		hh *= cnt;
	}

	if (cnt > 0) {
		hh += (cnt-1) * padding;
	}

	ww = ceil(ww);
	hh = ceil(hh);
	*w = ww;
	*h = hh;
	robwidget_set_area(rw, 0, 0, ww, hh);
#ifdef DEBUG_VBOX
	printf("VBOX request %d, %d (%.1f, %.1f)\n", ww, hh, rw->area.width, rw->area.height);
#endif
}

static void rvbox_size_allocate(RobWidget* rw, int w, int h) {
#ifdef DEBUG_VBOX
	printf("rvbox_size_allocate %s: %d, %d (%.1f, %.1f)\n",
			ROBWIDGET_NAME(rw), w, h, rw->area.width, rw->area.height);
#endif
	int padding = ((struct rob_container*)rw->self)->padding;
	bool expand = ((struct rob_container*)rw->self)->expand;
	if (h < rw->area.height) {
		printf(" !!! vbox packing error %d vs %.1f\n", h, rw->area.height);
		h = rw->area.height;
	}
	float xtra_space = 0;
	bool grow = FALSE;

	if (h > rw->area.height) {
		// check what widgets can expand..
		int cnt = 0;
		int exp = 0;
		for (unsigned int i=0; i < rw->childcount; ++i) {
			RobWidget * c = (RobWidget *) rw->children[i];
			if (c->hidden) continue;
			++cnt;
			if (!roblayout_can_expand(c)) continue;
			if (c->size_allocate) { ++exp;}
		}
		if (exp > 0) {
			/* divide extra space.. */
			xtra_space = (h - rw->area.height /*- (cnt-1) * padding*/) / (float) exp;
#ifdef DEBUG_VBOX
			printf("expand %d widgets by %.1f height (%d, %d)\n", exp, xtra_space, cnt, padding);
#endif
		} else if (!rw->position_set) {
			xtra_space = (h - rw->area.height) / 2.0;
			grow = TRUE;
#ifdef DEBUG_VBOX
			printf("grow self by %.1f height\n", xtra_space);
#endif
		} else {
#ifdef DEBUG_VBOX
			printf("don't grow\n");
#endif
		}
	}

	const int ww = /* expand-other (width in vbox)*/ 0 ? w : rw->area.width;

	/* allocate kids */
	for (unsigned int i=0; i < rw->childcount; ++i) {
		RobWidget * c = (RobWidget *) rw->children[i];
		if (c->hidden) continue;
		if (c->size_allocate) {
			c->size_allocate(c, ww, c->area.height + (grow ? 0 : floorf(xtra_space)));
		}
#ifdef DEBUG_VBOX
		printf("VBOXCHILD %d ('%s') use %.1fx%.1f\n", i,
				ROBWIDGET_NAME(rw->children[i]), c->area.width, c->area.height);
#endif
	}

	/* set position after allocation */
	float hh = grow ? xtra_space : 0;
	int ccnt = 0;
	for (unsigned int i=0; i < rw->childcount; ++i) {
		RobWidget * c = (RobWidget *) rw->children[i];
		if (c->hidden) continue;
		if (++ccnt != 1) { hh += padding; }
		if (c->position_set) {
			c->position_set(c, ww, c->area.height);
		} else {
			robwidget_position_set(c, ww, c->area.height);
		}
		c->area.x += floor((w - ww) / 2.0);
		c->area.y += floorf(hh);
		hh += c->area.height;
#ifdef DEBUG_VBOX
		printf("VBOXCHILD %d packed to %.1f+%.1f  %.1fx%.1f\n", i, c->area.x, c->area.y, c->area.width, c->area.height);
#endif
		if (c->redraw_pending) {
			queue_draw(c);
		}
	}
#ifdef DEBUG_VBOX
	if (grow) hh += xtra_space;
	if (hh != h) {
		printf("MISSED STH :) height: %.1f <> parent-h: %d\n", hh, h);
	}
#endif
	if (expand) {
		hh = h;
	} else {
		hh = rint(hh);
	}
	robwidget_set_area(rw, 0, 0, w, hh);
}

static void rob_vbox_child_pack(RobWidget *rw, RobWidget *chld, bool expand) {
	rcontainer_child_pack(rw, chld, expand);
}

static RobWidget * rob_vbox_new(bool homogeneous, int padding) {
	RobWidget * rw = robwidget_new(NULL);
	ROBWIDGET_SETNAME(rw, "vbox");
	rw->self = (struct rob_container*) calloc(1, sizeof(struct rob_container));
	((struct rob_container*)rw->self)->homogeneous = homogeneous;
	((struct rob_container*)rw->self)->padding = padding;
	((struct rob_container*)rw->self)->expand = TRUE;

	rw->size_request  = rvbox_size_request;
	rw->size_allocate = rvbox_size_allocate;

	rw->expose_event = rcontainer_expose_event;
	rw->mouseup      = rcontainer_mouseup;
	rw->mousedown    = rcontainer_mousedown;
	rw->mousemove    = rcontainer_mousemove;
	rw->mousemove    = rcontainer_mousemove;
	rw->mousescroll  = rcontainer_mousescroll;

	rw->area.x=0;
	rw->area.y=0;
	rw->area.width = 0;
	rw->area.height = 0;

	return rw;
}

static void dump_tbl_req(struct rob_table *rt) {
	unsigned int x,y;
	printf("---REQ---\n");
	printf("COLS: | ");
	for (x=0; x < rt->ncols; ++x) {
		printf(" *%4d* x  %4d  |", rt->cols[x].req_w, rt->cols[x].req_h);
	}
	printf("\n---------------\n");
	for (y=0; y < rt->nrows; ++y) {
		printf("ROW %d ||  %4d  x *%4d*\n", y, rt->rows[y].req_w, rt->rows[y].req_h);
	}
}

static void dump_tbl_acq(struct rob_table *rt) {
	unsigned int x,y;
	printf("---ALLOC---\n");
	printf("COLS: | ");
	for (x=0; x < rt->ncols; ++x) {
		printf(" *%4d* x  %4d  |", rt->cols[x].acq_w, rt->cols[x].acq_h);
	}
	printf("\n---------------\n");
	for (y=0; y < rt->nrows; ++y) {
		printf("ROW %d ||  %4d  x *%4d*\n", y, rt->rows[y].acq_w, rt->rows[y].acq_h);
	}
}

static void rob_box_destroy(RobWidget * rw) {
	free(rw->self);
	robwidget_destroy(rw);
}

/* table layout [jach] */

static void
rtable_size_request(RobWidget* rw, int *w, int *h) {
	assert(w && h);
	struct rob_table *rt = (struct rob_table*)rw->self;

	// reset
	for (unsigned int r=0; r < rt->nrows; ++r) {
		memset(&rt->rows[r], 0, sizeof(struct rob_table_field));
	}
	for (unsigned int c=0; c < rt->ncols; ++c) {
		memset(&rt->cols[c], 0, sizeof(struct rob_table_field));
	}

	// fill in childs
	for (unsigned int i=0; i < rt->nchilds; ++i) {
		int cw, ch;
		struct rob_table_child *tc = &rt->chld[i];
		RobWidget * c = (RobWidget *) tc->rw;
		if (c->hidden) continue;
		c->size_request(c, &cw, &ch);
		bool can_expand = roblayout_can_expand(c);
#ifdef DEBUG_TABLE
		printf("widget %d wants (%d x %d) x-span:%d y-span: %d\n", i, cw, ch, (tc->right - tc->left), (tc->bottom - tc->top));
#endif

		for (int span_x = tc->left; span_x < tc->right; ++span_x) {
			rt->cols[span_x].req_w = MAX(rt->cols[span_x].req_w, cw / (tc->right - tc->left)); // XXX
			rt->cols[span_x].req_h = MAX(rt->cols[span_x].req_h, ch); // unused -- homog
			if (can_expand) {
				rt->cols[span_x].is_expandable_x = TRUE;
			}
		}
		for (int span_y = tc->top; span_y < tc->bottom; ++span_y) {
			rt->rows[span_y].req_w = MAX(rt->rows[span_y].req_w, cw); // unused -- homog
			rt->rows[span_y].req_h = MAX(rt->rows[span_y].req_h, ch / (tc->bottom - tc->top)); // XXX
			if (can_expand) {
				rt->rows[span_y].is_expandable_y = TRUE;
			}
		}
		// XXX
		c->area.width = cw;
		c->area.height = ch;
	}
	// calc size of table
	int ww = 0;
	int hh = 0;
	for (unsigned int r=0; r < rt->nrows; ++r) {
		hh += rt->rows[r].req_h;
	}
	for (unsigned int c=0; c < rt->ncols; ++c) {
		ww += rt->cols[c].req_w;
	}

#if 0 // homogeneous
	// set area of children to detected max
	for (unsigned int i=0; i < rt->nchilds; ++i) {
		int cw = 0;
		int ch = 0;
		struct rob_table_child *tc = &rt->chld[i];
		RobWidget * c = (RobWidget *) tc->rw;
		if (c->hidden) continue;

		for (int span_x = tc->left; span_x < tc->right; ++span_x) {
			cw += rt->cols[span_x].req_w;
			//ch += rt->cols[span_x].req_h;
		}
		for (int span_y = tc->top; span_y < tc->bottom; ++span_y) {
			//cw += rt->rows[span_y].req_w;
			ch += rt->rows[span_y].req_h;
		}
		c->area.width = cw;
		c->area.height = ch;
	}
#endif
#ifdef DEBUG_TABLE
	dump_tbl_req(rt);
#endif

	ww = ceil(ww);
	hh = ceil(hh);
	*w = ww;
	*h = hh;
#ifdef DEBUG_TABLE
	printf("REQUEST TABLE SIZE: %d %d\n", ww, hh);
#endif
	robwidget_set_area(rw, 0, 0, ww, hh);
}

static void rtable_size_allocate(RobWidget* rw, int w, int h) {
	struct rob_table *rt = (struct rob_table*)rw->self;
#ifdef DEBUG_TABLE
	printf("table size_allocate %d, %d\n", w, h);
#endif
	if (h < rw->area.height || w < rw->area.width) {
		printf(" !!! table size request error. want %.1fx%.1f got %dx%d\n", rw->area.width, rw->area.height, w, h);
		h = rw->area.height;
	}

	float xtra_height = 0;
	float xtra_width = 0;

	if (h > rw->area.height) {
		int cnt = 0;
		int exp = 0;
#ifdef DEBUG_TABLE
		printf("---TABLE CAN EXPAND in height to %d\n", h);
#endif
		for (unsigned int r=0; r < rt->nrows; ++r) {
			if (rt->rows[r].req_h == 0) continue;
			++cnt;
			if (rt->rows[r].is_expandable_y) { ++exp; }
		}
		if (exp > 0) {
			xtra_height = (h - rw->area.height) / (float)exp;
#ifdef DEBUG_TABLE
			printf("table expand %d widgets by %.1f height\n", exp, xtra_height);
#endif
		} else {
#ifdef DEBUG_TABLE
			printf("table no grow %.1f height\n", xtra_height);
#endif
		}
	}

	if (w > rw->area.width) {
		int cnt = 0;
		int exp = 0;
#ifdef DEBUG_TABLE
		printf("TABLE CAN EXPAND in width to %d\n", w);
#endif
		for (unsigned int c=0; c < rt->ncols; ++c) {
			if (rt->cols[c].req_w == 0) continue;
			++cnt;
			if (rt->cols[c].is_expandable_x) { ++exp; }
		}
		if (exp > 0) {
			xtra_width = (w - rw->area.width) / (float)exp;
#ifdef DEBUG_TABLE
			printf("table expand %d widgets by %.1f width\n", exp, xtra_width);
#endif
		} else {
#ifdef DEBUG_TABLE
			printf("table no grow %.1f width\n", xtra_width);
#endif
		}
	}

	for (unsigned int i=0; i < rt->nchilds; ++i) {
		int cw = 0;
		int ch = 0;
		struct rob_table_child *tc = &rt->chld[i];
		RobWidget * c = (RobWidget *) tc->rw;
		if (c->hidden) continue;
#if 0
		for (int span_x = tc->left; span_x < tc->right; ++span_x) {
			cw += rt->cols[span_x].req_w;
		}
		for (int span_y = tc->top; span_y < tc->bottom; ++span_y) {
			ch += rt->rows[span_y].req_h;
		}
#else
		c->size_request(c, &cw, &ch);
#endif

#ifdef DEBUG_TABLE
		printf("widget %d wants (%d x %d) x-span:%d y-span: %d  %d, %d\n", i, cw, ch, (tc->right - tc->left), (tc->bottom - tc->top), tc->left, tc->top);
#endif

		if (c->size_allocate) {
			c->size_allocate(c,
					cw + floorf(xtra_width * (tc->right - tc->left)),
					ch + floorf(xtra_height * (tc->bottom - tc->top)));
#if 0
			/* or rather assume child swallowed it all */
			//c->area.width = cw + xtra_width * (tc->right - tc->left);
			//c->area.height = ch + xtra_height * (tc->bottom - tc->top);
#endif

			/* distribute evenly over span */
			ch /= (tc->bottom - tc->top);
			cw /= (tc->right - tc->left);
			for (int span_y = tc->top; span_y < tc->bottom; ++span_y) {
				rt->rows[span_y].acq_h = MAX(rt->rows[span_y].acq_h, ch + xtra_height);
			}
			for (int span_x = tc->left; span_x < tc->right; ++span_x) {
				rt->cols[span_x].acq_w = MAX(rt->cols[span_x].acq_w, cw + xtra_width);
			}
		} else {
			ch /= (tc->bottom - tc->top);
			cw /= (tc->right - tc->left);
			for (int span_y = tc->top; span_y < tc->bottom; ++span_y) {
				rt->rows[span_y].acq_h = MAX(rt->rows[span_y].acq_h, ch);
			}
			for (int span_x = tc->left; span_x < tc->right; ++span_x) {
				rt->cols[span_x].acq_w = MAX(rt->cols[span_x].acq_w, cw);
			}
		}
#ifdef DEBUG_TABLE
		dump_tbl_acq(rt);
		printf("TABLECHILD %d use %.1fx%.1f (field: %dx%d)\n", i, c->area.width, c->area.height, rt->cols[tc->left].acq_w, rt->rows[tc->top].acq_h);
#endif
	}

#ifdef DEBUG_TABLE
	dump_tbl_acq(rt);
#endif
	int max_w = 0;
	int max_h = 0;
	/* set position after allocation */
	for (unsigned int i=0; i < rt->nchilds; ++i) {
		int cw = 0;
		int ch = 0;
		int cx = 0;
		int cy = 0;
		struct rob_table_child *tc = &rt->chld[i];
		RobWidget * c = (RobWidget *) tc->rw;
		if (c->hidden) continue;
		for (int span_x = tc->left; span_x < tc->right; ++span_x) {
			cw += rt->cols[span_x].acq_w;
		}
		for (int span_y = tc->top; span_y < tc->bottom; ++span_y) {
			ch += rt->rows[span_y].acq_h;
		}
		for (int span_x = 0; span_x < tc->left; ++span_x) {
			cx += rt->cols[span_x].acq_w;
		}
		for (int span_y = 0; span_y < tc->top; ++span_y) {
			cy += rt->rows[span_y].acq_h;
		}
#ifdef DEBUG_TABLE
		printf("TABLECHILD %d avail %dx%d at %d+%d (wsize: %.1fx%.1f)\n", i, cw, ch, cx, cy, c->area.width, c->area.height);
#endif
#if 1
		if (c->size_allocate) {
			c->size_allocate(c, cw, ch);
#ifdef DEBUG_TABLE
		printf("TABLECHILD %d reloc %dx%d at %d+%d (wsize: %.1fx%.1f)\n", i, cw, ch, cx, cy, c->area.width, c->area.height);
#endif
		}
#endif
#if 1
		if (c->position_set) {
			c->position_set(c, cw, ch);
		} else {
			robwidget_position_set(c, cw, ch);
		}
		c->area.x += cx;
		c->area.y += cy;
#else
		c->area.x = cx;
		c->area.y = cy;
#endif

		if (c->area.x + c->area.width  > max_w) max_w = c->area.x + c->area.width;
		if (c->area.y + c->area.height > max_h) max_h = c->area.y + c->area.height;
#ifdef DEBUG_TABLE
		printf("TABLE %d packed to %.1f+%.1f  %.1fx%.1f\n", i, c->area.x, c->area.y, c->area.width, c->area.height);
#endif
		if (c->redraw_pending) {
			queue_draw(c);
		}
	}
#ifdef DEBUG_TABLE
	printf("TABLE PACKED total %dx%d  (given: %dx%d)\n", max_w, max_h, w, h);
	if (max_w > w || max_h > h) {
		printf("TABLE OVERFLOW total %dx%d  (given: %dx%d)\n", max_w, max_h, w, h);
	}
#endif
	robwidget_set_area(rw, (max_w - w) / 2.0, (max_h - h) / 2.0, max_w, max_h);
}

static void rob_table_attach(RobWidget *rw, RobWidget *chld,
		unsigned int left, unsigned int right, unsigned int top, unsigned int bottom,
		int xpadding, int ypadding
		) {
	assert(left < right);
	assert(top < bottom);

	rcontainer_child_pack(rw, chld, false);
	struct rob_table *rt = (struct rob_table*)rw->self;

	if (right >= rt->ncols) {
		rob_table_resize (rt, rt->nrows, right);
	}
	if (bottom >= rt->nrows) {
		rob_table_resize (rt, bottom, rt->ncols);
	}

	rt->chld = (struct rob_table_child*) realloc(rt->chld, (rt->nchilds + 1) * sizeof(struct rob_table_child));
	rt->chld[rt->nchilds].rw       = chld;
	rt->chld[rt->nchilds].left     = left;
	rt->chld[rt->nchilds].right    = right;
	rt->chld[rt->nchilds].top      = top;
	rt->chld[rt->nchilds].bottom   = bottom;
	rt->chld[rt->nchilds].xpadding = xpadding;
	rt->chld[rt->nchilds].ypadding = ypadding;
	rt->nchilds++;
}

static void rob_table_attach_defaults(RobWidget *rw, RobWidget *chld,
		unsigned int left, unsigned int right, unsigned int top, unsigned int bottom) {
	rob_table_attach(rw, chld, left, right, top, bottom, 0, 0);
}

static RobWidget * rob_table_new(int rows, int cols, bool homogeneous) {
	RobWidget * rw = robwidget_new(NULL);
	ROBWIDGET_SETNAME(rw, "tbl");
	rw->self = (struct rob_table*) calloc(1, sizeof(struct rob_table));
	struct rob_table *rt = (struct rob_table*)rw->self;
	rt->homogeneous = homogeneous;
	rt->expand = TRUE;
	rob_table_resize (rt, rows, cols);

	rw->size_request  = rtable_size_request;
	rw->size_allocate = rtable_size_allocate;

	rw->expose_event = rcontainer_expose_event;
	rw->mouseup      = rcontainer_mouseup;
	rw->mousedown    = rcontainer_mousedown;
	rw->mousemove    = rcontainer_mousemove;
	rw->mousemove    = rcontainer_mousemove;
	rw->mousescroll  = rcontainer_mousescroll;

	rw->area.x=0;
	rw->area.y=0;
	rw->area.width = 0;
	rw->area.height = 0;

	return rw;
}

static void rob_table_destroy(RobWidget * rw) {
	struct rob_table *rt = (struct rob_table*)rw->self;
	free(rt->chld);
	free(rt->rows);
	free(rt->cols);
	free(rw->self);
	robwidget_destroy(rw);
}

/* recursive childpos cache */
static void
rtoplevel_cache(RobWidget* rw, bool valid) {
	for (unsigned int i=0; i < rw->childcount; ++i) {
		RobWidget * c = (RobWidget *) rw->children[i];
		if (c->hidden) {
			valid= FALSE;
		}
		rtoplevel_cache(c, valid);
	}
	robwidget_position_cache(rw);
	rw->cached_position = valid;
}
