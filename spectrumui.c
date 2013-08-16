/* spectrum analyzer LV2 GUI
 *
 * Copyright 2012-2013 Robin Gareus <robin@gareus.org>
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

#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <pango/pango.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#define GM_TOP    ( 12.5f)
#define GM_LEFT   (  5.5f)
#define GM_GIRTH  ( 10.0f)
#define GM_WIDTH  ( 20.0f)

#define GM_HEIGHT (400.0f)
#define GM_TXT    (GM_HEIGHT - 52.0f)
#define GM_SCALE  (GM_TXT - GM_TOP - GM_TOP + 2.0)

#define MA_WIDTH  (25.0f)

#define MAX_CAIRO_PATH 32
#define NUM_METERS 31

#define	TOF ((GM_TOP           ) / GM_HEIGHT)
#define	BOF ((GM_TOP + GM_SCALE) / GM_HEIGHT)
#define	YVAL(x) ((GM_TOP + GM_SCALE - (x)) / GM_HEIGHT)
#define	YPOS(x) (GM_TOP + GM_SCALE - (x))

#define UINT_TO_RGB(u,r,g,b) { (*(r)) = ((u)>>16)&0xff; (*(g)) = ((u)>>8)&0xff; (*(b)) = (u)&0xff; }
#define UINT_TO_RGBA(u,r,g,b,a) { UINT_TO_RGB(((u)>>8),r,g,b); (*(a)) = (u)&0xff; }


static const char *freq_table [NUM_METERS] = {
	" 20 Hz",   " 25 Hz",  "31.5 Hz",
	" 40 Hz",   " 50 Hz",   " 63 Hz", " 80 Hz",
	"100 Hz",   "125 Hz",   "160 Hz",
	"200 Hz",   "250 Hz",   "315 Hz",
	"400 Hz",   "500 Hz",   "630 Hz", "800 Hz",
	" 1 kHz",  "1250 Hz",  "1.6 kHz",
	" 2 kHz",  "2.5 kHz",  "3150 Hz",
	" 4 kHz",   " 5 kHz",  "6.3 kHz", " 8 kHz",
	"10 kHz", "12.5 kHz",  "16 kHz",
	"20 kHz"
};

typedef struct {
	LV2_Handle instance;
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	GtkWidget* box;
	GtkWidget* align;
	GtkWidget* m0;
	GtkWidget* fader;

	cairo_surface_t* sf[NUM_METERS];
	cairo_surface_t* an[NUM_METERS];
	cairo_surface_t* ma[2];
	cairo_pattern_t* mpat;

	float val[NUM_METERS];
	float vui[NUM_METERS];

	float peak_val[NUM_METERS];
	int   peak_hold[NUM_METERS];

	bool disable_signals;
	float gain;

} SAUI;

/******************************************************************************
 * meter deflection
 */

static inline float
log_meter (float db)
{
         float def = 0.0f; /* Meter deflection %age */

         if (db < -70.0f) {
                 def = 0.0f;
         } else if (db < -60.0f) {
                 def = (db + 70.0f) * 0.25f;
         } else if (db < -50.0f) {
                 def = (db + 60.0f) * 0.5f + 2.5f;
         } else if (db < -40.0f) {
                 def = (db + 50.0f) * 0.75f + 7.5f;
         } else if (db < -30.0f) {
                 def = (db + 40.0f) * 1.5f + 15.0f;
         } else if (db < -20.0f) {
                 def = (db + 30.0f) * 2.0f + 30.0f;
         } else if (db < 6.0f) {
                 def = (db + 20.0f) * 2.5f + 50.0f;
         } else {
		 def = 115.0f;
	 }

	 /* 115 is the deflection %age that would be
	    when db=6.0. this is an arbitrary
	    endpoint for our scaling.
	 */

  return GM_SCALE * (def/115.0f);
}

static int deflect(float val) {
	int lvl = rint(log_meter(val));
	if (lvl < 2) lvl = 2;
	if (lvl >= GM_SCALE) lvl = GM_SCALE;
	return lvl;
}

/******************************************************************************
 * drawing helpers
 */
static void render_meter(SAUI*, int, int, int);

static void create_meter_pattern(SAUI* ui) {
	const int width = GM_WIDTH;
	const int height = GM_HEIGHT;

	int clr[10];
	float stp[4];

	stp[3] = deflect(0);
	stp[2] = deflect(-3);
	stp[1] = deflect(-9);
	stp[0] = deflect(-18);

	clr[0]=0x001188ff; clr[1]=0x00aa00ff;
	clr[2]=0x00ff00ff; clr[3]=0x00ff00ff;
	clr[4]=0xffff00ff; clr[5]=0xffff00ff;
	clr[6]=0xffaa00ff; clr[7]=0xffaa00ff;
	clr[8]=0xff0000ff; clr[9]=0xff0000ff;

	guint8 r,g,b,a;
	const double onep  =  1.0 / (double) GM_SCALE;
	const double softT =  2.0 / (double) GM_SCALE;
	const double softB =  2.0 / (double) GM_SCALE;

	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, height);

	cairo_pattern_add_color_stop_rgb (pat,  .0, .0 , .0, .0);
	cairo_pattern_add_color_stop_rgb (pat, TOF - onep,      .0 , .0, .0);
	cairo_pattern_add_color_stop_rgb (pat, TOF, .5 , .5, .5);

	// top/clip
	UINT_TO_RGBA (clr[9], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, TOF + onep,
	                                  r/255.0, g/255.0, b/255.0);

	// -0dB
	UINT_TO_RGBA (clr[8], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, YVAL(stp[3]) - softT,
	                                  r/255.0, g/255.0, b/255.0);
	UINT_TO_RGBA (clr[7], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, YVAL(stp[3]) + softB,
	                                  r/255.0, g/255.0, b/255.0);

	// -3dB || -2dB
	UINT_TO_RGBA (clr[6], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, YVAL(stp[2]) - softT,
	                                  r/255.0, g/255.0, b/255.0);
	UINT_TO_RGBA (clr[5], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, YVAL(stp[2]) + softB,
	                                  r/255.0, g/255.0, b/255.0);

	// -9dB
	UINT_TO_RGBA (clr[4], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, YVAL(stp[1]) - softT,
	                                  r/255.0, g/255.0, b/255.0);
	UINT_TO_RGBA (clr[3], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, YVAL(stp[1]) + softB,
	                                  r/255.0, g/255.0, b/255.0);

	// -18dB
	UINT_TO_RGBA (clr[2], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, YVAL(stp[0]) - softT,
	                                  r/255.0, g/255.0, b/255.0);
	UINT_TO_RGBA (clr[1], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, YVAL(stp[0]) + softB,
	                                  r/255.0, g/255.0, b/255.0);

	// -inf
	UINT_TO_RGBA (clr[0], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, BOF - 4 * onep - softT,
	                                  r/255.0, g/255.0, b/255.0);

	//Bottom
	cairo_pattern_add_color_stop_rgb (pat, BOF, .2 , .2, .2);
	cairo_pattern_add_color_stop_rgb (pat, BOF + onep, .0 , .0, .0);
	cairo_pattern_add_color_stop_rgb (pat, 1.0, .0 , .0, .0);

	if (!getenv("NO_METER_SHADE") || strlen(getenv("NO_METER_SHADE")) == 0) {
		cairo_pattern_t* shade_pattern = cairo_pattern_create_linear (0.0, 0.0, width, 0.0);
		cairo_pattern_add_color_stop_rgba (shade_pattern, (GM_LEFT-1.0) / GM_WIDTH,   0.0, 0.0, 0.0, 0.15);
		cairo_pattern_add_color_stop_rgba (shade_pattern, (GM_LEFT + GM_GIRTH * .35) / GM_WIDTH, 1.0, 1.0, 1.0, 0.10);
		cairo_pattern_add_color_stop_rgba (shade_pattern, (GM_LEFT + GM_GIRTH * .53) / GM_WIDTH, 0.0, 0.0, 0.0, 0.05);
		cairo_pattern_add_color_stop_rgba (shade_pattern, (GM_LEFT+1.0+GM_GIRTH) / GM_WIDTH,  0.0, 0.0, 0.0, 0.25);

		cairo_surface_t* surface;
		cairo_t* tc = 0;
		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
		tc = cairo_create (surface);
		cairo_set_source (tc, pat);
		cairo_rectangle (tc, 0, 0, width, height);
		cairo_fill (tc);
		cairo_pattern_destroy (pat);

		cairo_set_source (tc, shade_pattern);
		cairo_rectangle (tc, 0, 0, width, height);
		cairo_fill (tc);
		cairo_pattern_destroy (shade_pattern);

		// LED stripes
		cairo_save (tc);
		cairo_set_line_width(tc, 1.0);
		cairo_set_source_rgba(tc, .0, .0, .0, 0.4);
		//cairo_set_operator (tc, CAIRO_OPERATOR_SOURCE);
		for (float y=0.5; y < height; y+= 2.0) {
			cairo_move_to(tc, 0, y);
			cairo_line_to(tc, width, y);
			cairo_stroke (tc);
		}
		cairo_restore (tc);

		pat = cairo_pattern_create_for_surface (surface);
		cairo_destroy (tc);
		cairo_surface_destroy (surface);
	}

	ui->mpat= pat;
}

static void write_text(
		cairo_t* cr,
		const char *txt, const char * font,
		const float ang,
		const float x, const float y) {
	int tw, th;
	cairo_save(cr);

	PangoLayout * pl = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string(font);
	pango_layout_set_font_description(pl, fd);
	pango_font_description_free(fd);
	cairo_set_source_rgba (cr, .9, .9, .9, 1.0);
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, &tw, &th);
	cairo_translate (cr, x, y);
	if (ang != 0)  {
		cairo_rotate(cr, ang);
		cairo_translate (cr, -tw, 0);
	}  else {
	  cairo_translate (cr, -tw - 0.5, -th/2.0);
	}
	pango_cairo_layout_path(cr, pl);
	pango_cairo_show_layout(cr, pl);
	g_object_unref(pl);
	cairo_restore(cr);
	cairo_new_path (cr);
}

void rounded_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
  double degrees = M_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);
  cairo_arc (cr, x + r, y + h - r, r, 90 * degrees, 180 * degrees);
  cairo_arc (cr, x + r, y + r, r, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);
}

/******************************************************************************
 * Drawing
 */

static void alloc_annotations(SAUI* ui) {
#define FONT_LBL "Sans 08"
#define FONT_MTR "Sans 06"

#define BACKGROUND_COLOR(CR) \
	cairo_set_source_rgba (CR, .3, .3, .3, 1.0);

#define INIT_ANN_BG(VAR, WIDTH, HEIGHT) \
	VAR = cairo_image_surface_create (CAIRO_FORMAT_RGB24, WIDTH, HEIGHT); \
	cr = cairo_create (VAR);

#define INIT_BLACK_BG(VAR, WIDTH, HEIGHT) \
	INIT_ANN_BG(VAR, WIDTH, HEIGHT) \
	cairo_set_source_rgb (cr, .0, .0, .0); \
	cairo_rectangle (cr, 0, 0, WIDTH, WIDTH); \
	cairo_fill (cr);

	cairo_t* cr;
	for (int i = 0; i < NUM_METERS; ++i) {
		INIT_BLACK_BG(ui->an[i], 24, 64)
		write_text(cr, freq_table[i], FONT_LBL, -M_PI/2, 4, 0);
		cairo_destroy (cr);
	}

#define DO_THE_METER(DB, TXT) \
	write_text(cr,  TXT , FONT_MTR, 0, MA_WIDTH - 2, YPOS(deflect(DB)));

#define DO_THE_METRICS \
	DO_THE_METER(   3,  "+3dB") \
	DO_THE_METER(   0,  " 0dB") \
	DO_THE_METER(  -3,  "-3dB") \
	DO_THE_METER(  -9,  "-9dB") \
	DO_THE_METER( -15, "-15dB") \
	DO_THE_METER( -18, "-18dB") \
	DO_THE_METER( -20, "-20dB") \
	DO_THE_METER( -25, "-25dB") \
	DO_THE_METER( -30, "-30dB") \
	DO_THE_METER( -40, "-40dB") \
	DO_THE_METER( -50, "-50dB") \
	DO_THE_METER( -60, "-60dB") \

	INIT_ANN_BG(ui->ma[0], MA_WIDTH, GM_HEIGHT);
	BACKGROUND_COLOR(cr)
	cairo_rectangle (cr, 0, 0, MA_WIDTH, GM_HEIGHT);
	cairo_fill (cr);
	DO_THE_METRICS
	write_text(cr,  "dBFS" , FONT_MTR, 0, MA_WIDTH - 2, GM_TXT);
	cairo_destroy (cr);

	INIT_ANN_BG(ui->ma[1], MA_WIDTH, GM_HEIGHT)
	BACKGROUND_COLOR(cr)
	cairo_rectangle (cr, 0, 0, MA_WIDTH, GM_HEIGHT);
	cairo_fill (cr);
	DO_THE_METRICS
	write_text(cr,  "dBFS" , FONT_MTR, 0, MA_WIDTH - 2, GM_TXT);
	cairo_destroy (cr);
}

static void alloc_sf(SAUI* ui) {
	cairo_t* cr;
#define ALLOC_SF(VAR) \
	VAR = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, GM_WIDTH, GM_HEIGHT);\
	cr = cairo_create (VAR);\
	BACKGROUND_COLOR(cr) \
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);\
	cairo_rectangle (cr, 0, 0, GM_WIDTH, GM_HEIGHT);\
	cairo_fill (cr);

#define GAINLINE(DB) { \
		const float yoff = GM_TOP + GM_SCALE - deflect(DB); \
		cairo_move_to(cr, 0, yoff); \
		cairo_line_to(cr, GM_WIDTH, yoff); \
		cairo_stroke(cr); \
}

	for (int i = 0; i < NUM_METERS; ++i) {
		ALLOC_SF(ui->sf[i])
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_line_width(cr, 1.0);
		cairo_set_source_rgba (cr, .8, .8, .8, 1.0);
		GAINLINE(3);
		GAINLINE(0);
		GAINLINE(-3);
		GAINLINE(-9);
		GAINLINE(-15);
		GAINLINE(-18);
		GAINLINE(-20);
		GAINLINE(-25);
		GAINLINE(-30);
		GAINLINE(-40);
		GAINLINE(-50);
		GAINLINE(-60);
		cairo_destroy(cr);
		render_meter(ui, i, GM_SCALE, 2);
		ui->vui[i] = 2;
	}
}

static void render_meter(SAUI* ui, int i, int old, int new) {
	cairo_t* cr = cairo_create (ui->sf[i]);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

#ifdef FIXED_CLIPPING // XXX
	int t, h;
	if (old > new) {
		t = old;
		h = old - new;
	} else {
		t = new;
		h = new - old;
	}
	cairo_rectangle(cr, GM_LEFT -1 , GM_TOP + GM_SCALE - t - 2 , GM_GIRTH + 2, h+4);
	cairo_clip(cr);
#endif

	cairo_set_source_rgb (cr, .0, .0, .0);
	rounded_rectangle (cr, GM_LEFT-1, GM_TOP, GM_GIRTH+2, GM_SCALE, 6);
	cairo_fill_preserve(cr);
	cairo_clip(cr);

	cairo_set_source(cr, ui->mpat);
	cairo_rectangle (cr, GM_LEFT, GM_TOP + GM_SCALE - new - 1, GM_GIRTH, new + 1);
	cairo_fill(cr);

	/* border */
	cairo_reset_clip(cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	cairo_set_line_width(cr, 0.75);
	cairo_set_source_rgb (cr, .6, .6, .6);

	cairo_move_to(cr, GM_LEFT + GM_GIRTH/2, GM_TOP + GM_SCALE + 2);
	cairo_line_to(cr, GM_LEFT + GM_GIRTH/2, GM_TOP + GM_SCALE + 8);
	cairo_stroke(cr);

#ifdef FIXED_CLIPPING // XXX
	cairo_rectangle(cr, GM_LEFT -1 , GM_TOP + GM_SCALE - t - 2 , GM_GIRTH + 2, h+4);
	cairo_clip(cr);
#endif
	rounded_rectangle (cr, GM_LEFT-1, GM_TOP, GM_GIRTH+2, GM_SCALE, 6);
	cairo_stroke(cr);

	cairo_destroy(cr);
}

static gboolean expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	SAUI* ui = (SAUI*)handle;

	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	for (int i = 0; i < NUM_METERS ; ++i) {
		const int old = ui->vui[i];
		const int new = deflect(ui->val[i]);
		if (old == new) { continue; }
		ui->vui[i] = new;
		render_meter(ui, i, old, new);
	}

	cairo_set_operator (cr, CAIRO_OPERATOR_ATOP);

	/* metric areas */
	cairo_set_source_surface(cr, ui->ma[0], 0, 0);
	cairo_paint (cr);
	cairo_set_source_surface(cr, ui->ma[1], MA_WIDTH + GM_WIDTH * NUM_METERS, 0);
	cairo_paint (cr);

	/* meters */
	for (int i = 0; i < NUM_METERS ; ++i) {
		cairo_set_source_surface(cr, ui->sf[i], MA_WIDTH + GM_WIDTH * i, 0);
		cairo_paint (cr);
	}

	/* labels */
	cairo_set_operator (cr, CAIRO_OPERATOR_SCREEN);
	for (int i = 0; i < NUM_METERS ; ++i) {
		cairo_set_source_surface(cr, ui->an[i], MA_WIDTH + GM_WIDTH * i, GM_TXT);
		cairo_paint (cr);
	}

	cairo_destroy (cr);

	return TRUE;
}


/******************************************************************************
 * UI callbacks
 */

static gboolean set_gain(GtkRange* r, gpointer handle) {
	SAUI* ui = (SAUI*)handle;
	ui->gain = 4.0 - gtk_range_get_value(r);
	if (!ui->disable_signals) {
		ui->write(ui->controller, 4, sizeof(float), 0, (const void*) &ui->gain);
	}
	return TRUE;
}

static gboolean cb_expose(GtkWidget *w, gpointer handle) {
	SAUI* ui = (SAUI*)handle;
	gtk_widget_queue_draw(ui->m0);
	return TRUE;
}

/******************************************************************************
 * LV2 callbacks
 */

static LV2UI_Handle
instantiate(const LV2UI_Descriptor*   descriptor,
            const char*               plugin_uri,
            const char*               bundle_path,
            LV2UI_Write_Function      write_function,
            LV2UI_Controller          controller,
            LV2UI_Widget*             widget,
            const LV2_Feature* const* features)
{
	SAUI* ui = (SAUI*) calloc(1,sizeof(SAUI));
	*widget = NULL;

	ui->write      = write_function;
	ui->controller = controller;

	alloc_annotations(ui);
	create_meter_pattern(ui);
	alloc_sf(ui);

	for (int i=0; i < NUM_METERS ; ++i) {
		ui->val[i] = -70.0;
	}

	ui->disable_signals = false;

	ui->box   = gtk_hbox_new(FALSE, 2);
	ui->align = gtk_alignment_new(.5, .5, 0, 0);
	ui->m0    = gtk_drawing_area_new();
	ui->fader = gtk_vscale_new_with_range(0.0, 4.0, .001);

	/* fader init */
	gtk_scale_set_draw_value(GTK_SCALE(ui->fader), FALSE);
	gtk_range_set_value(GTK_RANGE(ui->fader), 3.0);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 0.0316, GTK_POS_RIGHT, "" /* -30dB */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 0.1,    GTK_POS_RIGHT, "-20dB");
	//gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 0.1258, GTK_POS_RIGHT, "" /*"-18db" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 0.2511, GTK_POS_RIGHT, "" /*"-12db" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 0.3548, GTK_POS_RIGHT, "" /* "-9dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 0.5012, GTK_POS_RIGHT, "-6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 0.7079, GTK_POS_RIGHT, "" /* "-3dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 1.0000, GTK_POS_RIGHT, "0dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 1.4125, GTK_POS_RIGHT, "" /* "+3dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 1.9952, GTK_POS_RIGHT, "+6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 2.8183, GTK_POS_RIGHT, "" /* "+9dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),4.0 - 3.9810, GTK_POS_RIGHT, "+12dB");

	gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), 2.0 * MA_WIDTH + NUM_METERS * GM_WIDTH, GM_HEIGHT);
	gtk_widget_set_size_request(ui->m0, 2.0 * MA_WIDTH + NUM_METERS * GM_WIDTH, GM_HEIGHT);
	gtk_widget_set_redraw_on_allocate(ui->m0, TRUE);

	/* layout */
	gtk_container_add(GTK_CONTAINER(ui->align), ui->m0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->align, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->fader, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);
	g_signal_connect (G_OBJECT (ui->fader), "value-changed", G_CALLBACK (set_gain), ui);

	gtk_widget_show_all(ui->box);
	*widget = ui->box;

	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	SAUI* ui = (SAUI*)handle;
	for (int i=0; i < NUM_METERS ; ++i) {
		cairo_surface_destroy(ui->sf[i]);
		cairo_surface_destroy(ui->an[i]);
	}
	cairo_pattern_destroy (ui->mpat);
	cairo_surface_destroy(ui->ma[0]);
	cairo_surface_destroy(ui->ma[1]);
	gtk_widget_destroy(ui->m0);
	gtk_widget_destroy(ui->fader);

	free(ui);
}

static void invalidate_meter(SAUI* ui, int mtr, float val) {
	const int old = deflect(ui->val[mtr]);
	const int new = deflect(val);
	if (old == new) {
		return;
	}
	ui->val[mtr] = val;
	int t, h;
	if (old > new) {
		t = old;
		h = old - new;
	} else {
		t = new;
		h = new - old;
	}

	gtk_widget_queue_draw_area(ui->m0,
			mtr * GM_WIDTH + MA_WIDTH + GM_LEFT - 1,
			GM_TOP + GM_SCALE - t - 1,
			GM_GIRTH + 2, h+3);
}

/******************************************************************************
 * handle data from backend
 */

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	SAUI* ui = (SAUI*)handle;
	if (format != 0) return;
	if (port_index == 4) {
		float v = *(float *)buffer;
		if (v >= 0 && v <= 4.0) {
			ui->disable_signals = true;
			gtk_range_set_value(GTK_RANGE(ui->fader), 4.0 - v);
			ui->disable_signals = false;
		}
	} else
	if (port_index > 4 && port_index < 5 + NUM_METERS) {
		invalidate_meter(ui, port_index -5, *(float *)buffer);
	}
}

/******************************************************************************
 * LV2 setup
 */

static const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2UI_Descriptor descriptor = {
	"http://gareus.org/oss/lv2/meters#spectrumui",
	instantiate,
	cleanup,
	port_event,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor*
lv2ui_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
