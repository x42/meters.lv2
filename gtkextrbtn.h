/* gtk extended radio button cairo widget
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

#ifndef _GTK_EXT_RBTN_H_
#define _GTK_EXT_RBTN_H_

#include <pthread.h>
#include "gtkextcbtn.h"

typedef struct {
	GtkExtCBtn *cbtn;
	void * cbtngrp;
	gboolean own_radiogrp;

	gboolean (*cb) (GtkWidget* w, gpointer handle);
	gpointer handle;
} GtkExtRBtn;

typedef struct {
	GtkExtRBtn **btn;
	unsigned int cnt;
	pthread_mutex_t _mutex;
} GtkExtRadioGrp;


static void btn_group_add_btn (GtkExtRadioGrp *g, GtkExtRBtn *btn) {
	pthread_mutex_lock (&g->_mutex);
	g->btn = realloc(g->btn, (g->cnt + 1) * sizeof(GtkExtRBtn*));
	g->btn[g->cnt] = btn;
	g->cnt++;
	pthread_mutex_unlock (&g->_mutex);
}

static void btn_group_remove_btn (GtkExtRadioGrp *g, GtkExtRBtn *btn) {
	pthread_mutex_lock (&g->_mutex);
	gboolean found = FALSE;
	for (unsigned int i = 0; i < g->cnt; ++i) {
		if (g->btn[i] == btn) {
			found = TRUE;
		}
		if (found && i+1 < g->cnt) {
			g->btn[i] = g->btn[i+1];
		}
	}
	g->cnt++;
	pthread_mutex_unlock (&g->_mutex);
}

static GtkExtRadioGrp * btn_group_new() {
	GtkExtRadioGrp *g = malloc(sizeof(GtkExtRadioGrp));
	g->btn=NULL;
	g->cnt=0;
	pthread_mutex_init (&g->_mutex, 0);
	return g;
}

static void btn_group_destroy(GtkExtRadioGrp *g) {
	pthread_mutex_destroy(&g->_mutex);
	free(g->btn);
	free(g);
}


static void btn_group_propagate_change (GtkExtRadioGrp *g, GtkExtRBtn *btn) {
	pthread_mutex_lock (&g->_mutex);
	for (unsigned int i = 0; i < g->cnt; ++i) {
		if (g->btn[i] == btn) continue;
		gtkext_cbtn_set_active(g->btn[i]->cbtn, FALSE);
	}
	pthread_mutex_unlock (&g->_mutex);
}

static gboolean btn_group_cbtn_callback(GtkWidget *w, gpointer handle) {
	GtkExtRBtn *d = (GtkExtRBtn *) handle;
	if (gtkext_cbtn_get_active(d->cbtn)) {
		btn_group_propagate_change((GtkExtRadioGrp*) d->cbtngrp, d);
	}
	if (d->cb) d->cb(gtkext_cbtn_widget(d->cbtn), d->handle);
	return TRUE;
}

/******************************************************************************
 * public functions
 */

static GtkExtRBtn * gtkext_rbtn_new(const char * txt, GtkExtRadioGrp *group) {
	GtkExtRBtn *d = (GtkExtRBtn *) malloc(sizeof(GtkExtRBtn));
	d->cbtn = gtkext_cbtn_new(txt, GBT_LED_RADIO, TRUE); // TODO custom LED mode
	d->cb = NULL;
	d->handle = NULL;

	if (!group) {
		d->own_radiogrp = TRUE;
		d->cbtngrp = (void*) btn_group_new();
	} else {
		d->own_radiogrp = FALSE;
		d->cbtngrp = group;
	}
	btn_group_add_btn((GtkExtRadioGrp*) d->cbtngrp, d);
	gtkext_cbtn_set_callback(d->cbtn, btn_group_cbtn_callback, d);
	return d;
}

static void gtkext_rbtn_destroy(GtkExtRBtn *d) {
	if (d->own_radiogrp) {
		btn_group_destroy((GtkExtRadioGrp*) d->cbtngrp);
	}
	gtkext_cbtn_destroy(d->cbtn);
	free(d);
}

static GtkWidget * gtkext_rbtn_widget(GtkExtRBtn *d) {
	return gtkext_cbtn_widget(d->cbtn);
}

static void * gtkext_rbtn_group(GtkExtRBtn *d) {
	assert(d->cbtngrp);
	return d->cbtngrp;
}

static void gtkext_rbtn_set_callback(GtkExtRBtn *d, gboolean (*cb) (GtkWidget* w, gpointer handle), gpointer handle) {
	d->cb = cb;
	d->handle = handle;
}

static void gtkext_rbtn_set_active(GtkExtRBtn *d, gboolean v) {
	gtkext_cbtn_set_active(d->cbtn, v);
}

static void gtkext_rbtn_set_sensitive(GtkExtRBtn *d, gboolean s) {
	gtkext_cbtn_set_sensitive(d->cbtn, s);
}

static gboolean gtkext_rbtn_get_active(GtkExtRBtn *d) {
	return gtkext_cbtn_get_active(d->cbtn);
}
#endif
