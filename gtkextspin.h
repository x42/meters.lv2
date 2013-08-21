/* gtk dial with numeric display widget
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

#ifndef _GTK_EXT_SPIN_H_
#define _GTK_EXT_SPIN_H_

#include <gtk/gtk.h>
#include <cairo/cairo.h>

#include "gtkextdial.h"
#include "gtkextlbl.h"

#define GSP_WIDTH 25
#define GSP_HEIGHT 29
#define GSP_RADIUS 10
#define GSP_CX 12.5
#define GSP_CY 12.5

typedef struct {
	GtkExtDial *dial;
	GtkWidget* b;
	GtkWidget* c;
	GtkExtLbl* lbl_r;
	GtkExtLbl* lbl_l;

	gboolean sensitive;
	gboolean prelight;
	char prec_fmt[8];

	gboolean (*cb) (GtkWidget* w, gpointer handle);
	gpointer handle;
	int lbl;
} GtkExtSpin;

static gboolean gtkext_spin_enter_notify(GtkWidget *w, GdkEvent *event, gpointer handle) {
	GtkExtSpin * d = (GtkExtSpin *)handle;
	if (!d->prelight) {
		d->prelight = TRUE;
	}
	return FALSE;
}
static gboolean gtkext_spin_leave_notify(GtkWidget *w, GdkEvent *event, gpointer handle) {
	GtkExtSpin * d = (GtkExtSpin *)handle;
	if (d->prelight) {
		d->prelight = FALSE;
	}
	return FALSE;
}

static gboolean gtkext_spin_callback(GtkWidget *w, gpointer handle) {
	GtkExtSpin *d = (GtkExtSpin *) handle;
	char buf[32];
	snprintf(buf, 32, d->prec_fmt, gtkext_dial_get_value(d->dial));
	buf[31] = '\0';
	if (d->lbl & 1) gtkext_lbl_set_text(d->lbl_l, buf);
	if (d->lbl & 2) gtkext_lbl_set_text(d->lbl_r, buf);
	if (d->cb) d->cb(gtkext_dial_widget(d->dial), d->handle);
	return TRUE;
}

/******************************************************************************
 * public functions
 */

static void gtkext_spin_set_digits(GtkExtSpin *d, int prec) {
	if (prec > 4) prec = 4;
	if (prec <= 0) {
		sprintf(d->prec_fmt,"%%.0f");
	} else {
		sprintf(d->prec_fmt,"%%.%df", prec);
	}
}

static GtkExtSpin * gtkext_spin_new(float min, float max, float step) {
	GtkExtSpin *d = (GtkExtSpin *) malloc(sizeof(GtkExtSpin));

	gtkext_spin_set_digits(d, - floorf(log10f(step)));

	d->sensitive = TRUE;
	d->prelight = FALSE;
	d->cb = NULL;
	d->handle = NULL;
	d->lbl = 2;

	d->dial = gtkext_dial_new_with_size(min, max, step,
			GSP_WIDTH, GSP_HEIGHT, GSP_CX, GSP_CY, GSP_RADIUS);

	gtkext_dial_set_callback(d->dial, gtkext_spin_callback, d);

	d->lbl_r = gtkext_lbl_new("");
	d->lbl_l = gtkext_lbl_new("");
	d->b = gtk_hbox_new(FALSE, 2);
	d->c = gtk_alignment_new(.5, .5, 0, 0);
	gtk_box_pack_start(GTK_BOX(d->b), gtkext_lbl_widget(d->lbl_l), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(d->b), gtkext_dial_widget(d->dial), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(d->b), gtkext_lbl_widget(d->lbl_r), FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(d->c), d->b);
	gtk_widget_show_all(d->c);

	gtkext_spin_callback(0, d); // set value
	return d;
}

static void gtkext_spin_destroy(GtkExtSpin *d) {
	gtkext_dial_destroy(d->dial);
	gtkext_lbl_destroy(d->lbl_r);
	gtkext_lbl_destroy(d->lbl_l);
	gtk_widget_destroy(d->b);
	gtk_widget_destroy(d->c);
	free(d);
}

static void gtkext_spin_set_alignment(GtkExtSpin *d, float x, float y) {
	gtk_alignment_set(GTK_ALIGNMENT(d->c), x, y, 0, 0);
}

static void gtkext_spin_label_width(GtkExtSpin *d, float left, float right) {
	gtkext_lbl_set_min_geometry(d->lbl_l, (float) left, 0);
	gtkext_lbl_set_min_geometry(d->lbl_r, (float) right, 0);
}

static void gtkext_spin_set_label_pos(GtkExtSpin *d, int p) {
	d->lbl = p&3;
	if (!(d->lbl & 1)) gtkext_lbl_set_text(d->lbl_l, "");
	if (!(d->lbl & 2)) gtkext_lbl_set_text(d->lbl_r, "");
}

static GtkWidget * gtkext_spin_widget(GtkExtSpin *d) {
	return d->c;
}

static void gtkext_spin_set_callback(GtkExtSpin *d, gboolean (*cb) (GtkWidget* w, gpointer handle), gpointer handle) {
	d->cb = cb;
	d->handle = handle;
}

static void gtkext_spin_set_value(GtkExtSpin *d, float v) {
	gtkext_dial_set_value(d->dial, v);
}

static void gtkext_spin_set_sensitive(GtkExtSpin *d, gboolean s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		gtkext_lbl_set_sensitive(d->lbl_r, s);
		gtkext_lbl_set_sensitive(d->lbl_l, s);
	}
	gtkext_dial_set_sensitive(d->dial, s);
}

static float gtkext_spin_get_value(GtkExtSpin *d) {
	return gtkext_dial_get_value(d->dial);
}
#endif
