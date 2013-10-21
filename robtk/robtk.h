/* robwidget - gtk2 & GL wrapper
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
#ifndef _ROBTK_H_
#define _ROBTK_H_

#include <assert.h>
#include <pthread.h>
#include <string.h>

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

/*****************************************************************************/

typedef struct {
	int x; int y;
	int state;
	int direction; // scroll
} RobTkBtnEvent;

#ifndef ROBTK_MOD_SHIFT
#error backend implementation misses ROBTK_MOD_SHIFT
#endif

#ifndef ROBTK_MOD_CTRL
#error backend implementation misses ROBTK_MOD_CTRL
#endif

enum {
	ROBTK_SCROLL_ZERO,
	ROBTK_SCROLL_UP,
	ROBTK_SCROLL_DOWN,
	ROBTK_SCROLL_LEFT,
	ROBTK_SCROLL_RIGHT
};

enum LVGLResize {
	LVGL_ZOOM_TO_ASPECT,
	LVGL_LAYOUT_TO_FIT,
	LVGL_CENTER,
	LVGL_TOP_LEFT,
};

typedef struct _robwidget {
	void *self; // user-handle for the actual (wrapped) widget

	/* required */
	bool (*expose_event) (struct _robwidget* handle, cairo_t* cr, cairo_rectangle_t *ev);
	void (*size_request) (struct _robwidget* handle, int *w, int *h);

	/* optional */
	void (*position_set) (struct _robwidget* handle, int pw, int ph);
	void (*size_allocate) (struct _robwidget* handle, int pw, int ph);
	/* optional -- hybrid GL+cairo scaling */
	void (*size_limit) (struct _robwidget* handle, int *pw, int *ph);
	void (*size_default) (struct _robwidget* handle, int *pw, int *ph);

	/* optional */
	struct _robwidget* (*mousedown)    (struct _robwidget*, RobTkBtnEvent *event);
	struct _robwidget* (*mouseup)      (struct _robwidget*, RobTkBtnEvent *event);
	struct _robwidget* (*mousemove)    (struct _robwidget*, RobTkBtnEvent *event);
	struct _robwidget* (*mousescroll)  (struct _robwidget*, RobTkBtnEvent *event);
	void               (*enter_notify) (struct _robwidget*);
	void               (*leave_notify) (struct _robwidget*);

	/* internal - GL */
#ifndef GTK_BACKEND
	void* top;
	struct _robwidget* parent;
	struct _robwidget **children;
	unsigned int childcount;

	bool redraw_pending; // queue_draw_*() failed (during init or top-levelresize)
	bool resized; // full-redraw --containers after resize
	bool hidden; // don't display, skip in layout and events
#endif
	float xalign, yalign; // unused in GTK
	cairo_rectangle_t area; // allocated pos + size
	cairo_rectangle_t trel; // cached pos + size relative to top widget
	bool cached_position;

	/* internal - GTK */
#ifdef GTK_BACKEND
	GtkWidget *m0;
	GtkWidget *c;
#endif

	char name[12]; // debug with ROBWIDGET_NAME()
} RobWidget;


/*****************************************************************************/
/* provided by host */

static void resize_self(RobWidget *rw); // dangerous :) -- never call from expose_event
static void resize_toplevel(RobWidget *rw, int w, int h); // ditto
static void queue_draw(RobWidget *);
static void queue_draw_area(RobWidget *, int, int, int, int);
static void queue_tiny_area(RobWidget *rw, float x, float y, float w, float h);

static RobWidget * robwidget_new(void *);
static void robwidget_destroy(RobWidget *rw);

/*****************************************************************************/
/* static plugin singletons */

static LV2UI_Handle
instantiate(
		void *const               ui_toplevel,
		const LV2UI_Descriptor*   descriptor,
		const char*               plugin_uri,
		const char*               bundle_path,
		LV2UI_Write_Function      write_function,
		LV2UI_Controller          controller,
		RobWidget**               widget,
		const LV2_Feature* const* features);

static void
cleanup(LV2UI_Handle handle);

static enum LVGLResize
plugin_scale_mode(LV2UI_Handle handle);

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer);

static const void *
extension_data(const char* uri);

#define ROBWIDGET_NAME(RW) ( ((RobWidget*)(RW))->name[0] ? (const char*) (((RobWidget*)(RW))->name) : (const char*) "???")
#define ROBWIDGET_SETNAME(RW, TXT) strcpy(((RobWidget*)(RW))->name, TXT); // max 11 chars

/******************************************************************************/
// utils //

static bool rect_intersect(const cairo_rectangle_t *r1, const cairo_rectangle_t *r2){
	float dest_x, dest_y;
	float dest_x2, dest_y2;

	dest_x  = MAX (r1->x, r2->x);
	dest_y  = MAX (r1->y, r2->y);
	dest_x2 = MIN (r1->x + r1->width, r2->x + r2->width);
	dest_y2 = MIN (r1->y + r1->height, r2->y + r2->height);

	return (dest_x2 > dest_x && dest_y2 > dest_y);
}

static bool rect_intersect_a(const cairo_rectangle_t *r1, const float x, const float y, const float w, const float h) {
	cairo_rectangle_t r2 = { x, y, w, h};
	return rect_intersect(r1, &r2);
}

static void rect_combine(const cairo_rectangle_t *r1, const cairo_rectangle_t *r2, cairo_rectangle_t *dest) {
	double dest_x, dest_y;
	dest_x = MIN (r1->x, r2->x);
	dest_y = MIN (r1->y, r2->y);
	dest->width  = MAX (r1->x + r1->width,  r2->x + r2->width)  - dest_x;
	dest->height = MAX (r1->y + r1->height, r2->y + r2->height) - dest_y;
	dest->x = dest_x;
	dest->y = dest_y;
}

#include "rtk/style.h"
#include "rtk/common.h"

#ifdef GTK_BACKEND

#include "gtk2/common_cgtk.h"
#include "gtk2/robwidget_gtk.h"

#else

#include "gl/common_cgl.h"
#include "gl/robwidget_gl.h"
#include "gl/layout.h"

#endif

#include "widgets/robtk_checkbutton.h"
#include "widgets/robtk_dial.h"
#include "widgets/robtk_label.h"
#include "widgets/robtk_pushbutton.h"
#include "widgets/robtk_radiobutton.h"
#include "widgets/robtk_scale.h"
#include "widgets/robtk_separator.h"
#include "widgets/robtk_spinner.h"
#include "widgets/robtk_xyplot.h"

#endif
