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
#define MTR_URI "http://gareus.org/oss/lv2/meters#"

#define GM_TOP    (ui->display_freq ? 12.5f : 22.5f)
#define GM_LEFT   (ui->display_freq ?  1.5f :  8.5f)
#define GM_GIRTH  (ui->display_freq ?  8.0f : 12.0f)
#define GM_WIDTH  (ui->display_freq ? 13.0f : 28.0f)

#define GM_HEIGHT (400.0f)
#define GM_TXT    (GM_HEIGHT - (ui->display_freq ? 52.0f : 16.0f))
#define GM_SCALE  (GM_TXT - GM_TOP - GM_TOP + 2.0)

#define MA_WIDTH  (30.0f)

#define MAX_CAIRO_PATH 32
#define MAX_METERS 31

#define	TOF ((GM_TOP           ) / GM_HEIGHT)
#define	BOF ((GM_TOP + GM_SCALE) / GM_HEIGHT)
#define	YVAL(x) ((GM_TOP + GM_SCALE - (x)) / GM_HEIGHT)
#define	YPOS(x) (GM_TOP + GM_SCALE - (x))

#define UINT_TO_RGB(u,r,g,b) { (*(r)) = ((u)>>16)&0xff; (*(g)) = ((u)>>8)&0xff; (*(b)) = (u)&0xff; }
#define UINT_TO_RGBA(u,r,g,b,a) { UINT_TO_RGB(((u)>>8),r,g,b); (*(a)) = (u)&0xff; }

#define GAINSCALE(x) (40.0 -(x) * 4.0)
#define INV_GAINSCALE(x) (10.0 -(x) / 4.0)

static const char *freq_table [] = {
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
	GtkWidget* c_box;
	GtkWidget* fader;
	GtkWidget* spn_attack;
	GtkWidget* spn_decay;
	GtkWidget* lbl_attack;
	GtkWidget* lbl_decay;

	cairo_surface_t* sf[MAX_METERS];
	cairo_surface_t* an[MAX_METERS];
	cairo_surface_t* ma[2];
	cairo_pattern_t* mpat;

	float val[MAX_METERS];
	int   vui[MAX_METERS];

	float peak_val[MAX_METERS];
	int   peak_vis[MAX_METERS];

	bool disable_signals;
	float gain;
	uint32_t num_meters;
	bool display_freq;
	bool reset_toggle;
	bool initialized;

	float cache_sf;
	float cache_ma;
	int highlight;

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

  return (def/115.0f);
}

static int deflect(SAUI* ui, float val) {
	int lvl = rint(GM_SCALE * log_meter(val));
	if (lvl < 2) lvl = 2;
	if (lvl >= GM_SCALE) lvl = GM_SCALE;
	return lvl;
}

/******************************************************************************
 * drawing helpers
 */
static void render_meter(SAUI*, int, int, int, int, int);

static void create_meter_pattern(SAUI* ui) {
	const int width = GM_WIDTH;
	const int height = GM_HEIGHT;

	int clr[10];
	float stp[4];

	stp[3] = deflect(ui, 0);
	stp[2] = deflect(ui, -3);
	stp[1] = deflect(ui, -9);
	stp[0] = deflect(ui, -18);

	clr[0]=0x008844ff; clr[1]=0x00bb00ff;
	clr[2]=0x00ff00ff; clr[3]=0x00ff00ff;
	clr[4]=0xfff000ff; clr[5]=0xfff000ff;
	clr[6]=0xff8000ff; clr[7]=0xff8000ff;
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
	cairo_pattern_add_color_stop_rgb (pat, BOF, .1 , .1, .1);
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
		const float ang, const int align,
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
	}  else {
	}
	switch(align) {
		default:
			cairo_translate (cr, -tw/2.0 - 0.5, -th/2.0);
			break;
		case 1:
			cairo_translate (cr, -tw, 0);
			break;
		case 2:
			cairo_translate (cr, -tw - 0.5, -th/2.0);
			break;
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
#define FONT_VAL "Mono 07"
#define FONT_SPK "Mono 08"

#define BACKGROUND_COLOR(CR) \
	cairo_set_source_rgba (CR, 84/255.0, 85/255.0, 93/255.0, 1.0);

#define INIT_ANN_BG(VAR, WIDTH, HEIGHT) \
	if (!VAR) \
	VAR = cairo_image_surface_create (CAIRO_FORMAT_RGB24, WIDTH, HEIGHT); \
	cr = cairo_create (VAR);

#define INIT_BLACK_BG(VAR, WIDTH, HEIGHT) \
	INIT_ANN_BG(VAR, WIDTH, HEIGHT) \
	cairo_set_source_rgb (cr, .0, .0, .0); \
	cairo_rectangle (cr, 0, 0, WIDTH, WIDTH); \
	cairo_fill (cr);

	cairo_t* cr;
	if (ui->display_freq) {
		/* frequecy table */
		for (int i = 0; i < ui->num_meters; ++i) {
			INIT_BLACK_BG(ui->an[i], 24, 64)
			write_text(cr, freq_table[i], FONT_LBL, -M_PI/2, 1, -1, 0);
			cairo_destroy (cr);
		}
	}
}

static void realloc_metrics(SAUI* ui) {
	const float dboff = ui->gain > 0.001 ? 20.0 * log10f(ui->gain) : -60;
	if (rint(ui->cache_ma * 5) == rint(dboff * 5)) {
		return;
	}
	ui->cache_ma = dboff;
	cairo_t* cr;
#define DO_THE_METER(DB, TXT) \
	if (dboff + DB < 6.0 && dboff + DB >= -60) \
	write_text(cr,  TXT , FONT_MTR, 0, 2, MA_WIDTH - 3, YPOS(deflect(ui, dboff + DB)));

#define DO_THE_METRICS \
	DO_THE_METER(  18, "+18dB") \
	DO_THE_METER(  15, "+15dB") \
	DO_THE_METER(   9,  "+9dB") \
	DO_THE_METER(   6,  "+6dB") \
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
	write_text(cr,  ui->display_freq ? "dBFS" : "dBTP", FONT_MTR, 0, 2, MA_WIDTH - 5, GM_TXT - 10);
	cairo_destroy (cr);

	INIT_ANN_BG(ui->ma[1], MA_WIDTH, GM_HEIGHT)
	BACKGROUND_COLOR(cr)
	cairo_rectangle (cr, 0, 0, MA_WIDTH, GM_HEIGHT);
	cairo_fill (cr);
	DO_THE_METRICS
	write_text(cr,  ui->display_freq ? "dBFS" : "dBTP", FONT_MTR, 0, 2, MA_WIDTH - 5, GM_TXT - 10);
	cairo_destroy (cr);
}

static void prepare_metersurface(SAUI* ui) {
	const float dboff = ui->gain > 0.001 ? 20.0 * log10f(ui->gain) : -60;

	if (rint(ui->cache_sf * 5) == rint(dboff * 5)) {
		return;
	}
	ui->cache_sf = dboff;

	cairo_t* cr;
#define ALLOC_SF(VAR) \
	if (!VAR) \
	VAR = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, GM_WIDTH, GM_HEIGHT);\
	cr = cairo_create (VAR);\
	BACKGROUND_COLOR(cr) \
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);\
	cairo_rectangle (cr, 0, 0, GM_WIDTH, GM_HEIGHT);\
	cairo_fill (cr);

#define GAINLINE(DB) \
	if (dboff + DB < 5.99) { \
		const float yoff = GM_TOP + GM_SCALE - deflect(ui, dboff + DB); \
		cairo_move_to(cr, 0, yoff); \
		cairo_line_to(cr, GM_WIDTH, yoff); \
		cairo_stroke(cr); \
}

	for (int i = 0; i < ui->num_meters; ++i) {
		ALLOC_SF(ui->sf[i])

		/* metric background */
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_line_width(cr, 1.0);
		cairo_set_source_rgba (cr, .8, .8, .8, 1.0);
		GAINLINE(18);
		GAINLINE(15);
		GAINLINE(9);
		GAINLINE(6);
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

		render_meter(ui, i, GM_SCALE, 2, 0, 0);
		ui->vui[i] = 2;
		ui->peak_vis[i] = 0;
	}
}

static void render_meter(SAUI* ui, int i, int old, int new, int m_old, int m_new) {
	cairo_t* cr = cairo_create (ui->sf[i]);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

	cairo_set_source_rgb (cr, .0, .0, .0);
	rounded_rectangle (cr, GM_LEFT-1, GM_TOP, GM_GIRTH+2, GM_SCALE, 6);
	cairo_fill_preserve(cr);
	cairo_clip(cr);

	cairo_set_source(cr, ui->mpat);
	cairo_rectangle (cr, GM_LEFT, GM_TOP + GM_SCALE - new - 1, GM_GIRTH, new + 1);
	cairo_fill(cr);

	/* peak hold */
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle (cr, GM_LEFT, GM_TOP + GM_SCALE - m_new - 0.5, GM_GIRTH, 3);
#if 1
	cairo_fill_preserve (cr);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.2);
#else
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.3);
#endif
	cairo_fill(cr);

	/* border */
	cairo_reset_clip(cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	cairo_set_line_width(cr, 0.75);
	cairo_set_source_rgb (cr, .6, .6, .6);

	cairo_move_to(cr, GM_LEFT + GM_GIRTH/2, GM_TOP + GM_SCALE + 2);
	cairo_line_to(cr, GM_LEFT + GM_GIRTH/2, GM_TOP + GM_SCALE + 8);
	cairo_stroke(cr);

	rounded_rectangle (cr, GM_LEFT-1, GM_TOP, GM_GIRTH+2, GM_SCALE, 6);
	cairo_stroke(cr);

	cairo_destroy(cr);
}

static gboolean expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	SAUI* ui = (SAUI*)handle;

	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	for (int i = 0; i < ui->num_meters ; ++i) {
		const int old = ui->vui[i];
		const int new = deflect(ui, ui->val[i]);
		const int m_old = ui->peak_vis[i];
		const int m_new = deflect(ui, ui->peak_val[i]);
		if (old == new && m_old == m_new) { continue; }
		ui->vui[i] = new;
		ui->peak_vis[i] = m_new;
		render_meter(ui, i, old, new, m_old, m_new);
	}

	cairo_set_operator (cr, CAIRO_OPERATOR_ATOP);

	/* metric areas */
	cairo_set_source_surface(cr, ui->ma[0], 0, 0);
	cairo_paint (cr);
	cairo_set_source_surface(cr, ui->ma[1], MA_WIDTH + GM_WIDTH * ui->num_meters, 0);
	cairo_paint (cr);

	/* meters */
	for (int i = 0; i < ui->num_meters ; ++i) {
		cairo_set_source_surface(cr, ui->sf[i], MA_WIDTH + GM_WIDTH * i, 0);
		cairo_paint (cr);
	}

	/* labels */
	if (!ui->display_freq) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		for (int i = 0; i < ui->num_meters ; ++i) {
			char buf[24];
			cairo_save(cr);
			rounded_rectangle (cr, MA_WIDTH + GM_WIDTH * i + 2, 3, GM_WIDTH-4, GM_TOP-8, 4);
			if (ui->peak_val[i] >= -1.0) {
				cairo_set_source_rgba (cr, .6, .0, .0, 1.0);
			} else {
				cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
			}
			cairo_fill_preserve (cr);
			cairo_set_line_width(cr, 0.75);
			cairo_set_source_rgba (cr, .6, .6, .6, 1.0);
			cairo_stroke_preserve (cr);
			cairo_clip (cr);

			if ( ui->peak_val[i] <= -10.0) {
				//sprintf(buf, "%d", (int) rintf(ui->peak_val[i]));
				sprintf(buf, "%.0f ", ui->peak_val[i]);
			} else {
				sprintf(buf, "%+.1f", ui->peak_val[i]);
			}
			write_text(cr, buf, FONT_VAL, 0, 2, MA_WIDTH + GM_WIDTH * i + GM_WIDTH - 5, GM_TOP / 2);
			cairo_restore(cr);

			cairo_save(cr);
			rounded_rectangle (cr, MA_WIDTH + GM_WIDTH * i + 2, GM_TXT - 11.5, GM_WIDTH-4, 15, 4);
			cairo_fill_preserve (cr);
			cairo_set_line_width(cr, 0.75);
			cairo_set_source_rgba (cr, .6, .6, .6, 1.0);
			cairo_stroke_preserve (cr);
			cairo_clip (cr);
			sprintf(buf, "%+.1f", ui->val[i]);
			write_text(cr, buf, FONT_VAL, 0, 2, MA_WIDTH + GM_WIDTH * i + GM_WIDTH - 5, GM_TXT - 4);
			cairo_restore(cr);
		}
	}

	/* labels */
	cairo_set_operator (cr, CAIRO_OPERATOR_SCREEN);
	for (int i = 0; i < ui->num_meters ; ++i) {
		cairo_set_source_surface(cr, ui->an[i], MA_WIDTH + GM_WIDTH * i, GM_TXT);
		cairo_paint (cr);
	}

	/* highlight */
	if (ui->highlight >= 0 && ui->highlight < ui->num_meters) {
		const float dboff = ui->gain > 0.001 ? 20.0 * log10f(ui->gain) : -60;
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		const int i = ui->highlight;
		char buf[32];
		sprintf(buf, "%s\nc:%+5.1f\np:%+5.1f",
				freq_table[i], ui->val[i] - dboff, ui->peak_val[i] - dboff);
		cairo_save(cr);
		cairo_set_line_width(cr, 0.75);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
		cairo_move_to(cr, MA_WIDTH + GM_WIDTH * i + GM_LEFT + GM_GIRTH/2, GM_TXT - 9.5);
		cairo_line_to(cr, MA_WIDTH + GM_WIDTH * i + GM_LEFT + GM_GIRTH/2, GM_TXT - 4.5);
		cairo_stroke(cr);
		rounded_rectangle (cr, MA_WIDTH + GM_WIDTH * i + GM_WIDTH/2 - 32, GM_TXT -4.5, 64, 46, 3);
		cairo_set_source_rgba (cr, 0, 0, 0, .8);
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, .6, .6, .6, 1.0);
		cairo_stroke_preserve (cr);
		cairo_clip (cr);
		write_text(cr, buf, FONT_SPK, 0, 0, MA_WIDTH + GM_WIDTH * i + GM_WIDTH/2, GM_TXT + 18);
		cairo_restore(cr);
	}

	cairo_destroy (cr);

	return TRUE;
}


/******************************************************************************
 * UI callbacks
 */

static gboolean cb_reset_peak(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	SAUI* ui = (SAUI*)handle;
	if (!ui->display_freq) {
		/* reset peak-hold in backend
		 * -- use unused reflevel in dBTP to notify plugin
		 */
		ui->reset_toggle = !ui->reset_toggle;
		float temp = ui->reset_toggle ? 1.0 : 0.0;
		ui->write(ui->controller, 0, sizeof(float), 0, (const void*) &temp);
	}
	for (int i=0; i < ui->num_meters ; ++i) {
		ui->peak_val[i] = -70;
	}
	gtk_widget_queue_draw(ui->m0);
	return TRUE;
}

static gboolean set_gain(GtkRange* r, gpointer handle) {
	SAUI* ui = (SAUI*)handle;
	float oldgain = ui->gain;
	ui->gain = INV_GAINSCALE(gtk_range_get_value(r));
#if 1
	if (ui->gain <= .2511) ui->gain = .2511;
	if (ui->gain >= 10.0) ui->gain = 10.0;
#endif
	if (oldgain == ui->gain) return TRUE;
	if (!ui->disable_signals) {
		ui->write(ui->controller, 4, sizeof(float), 0, (const void*) &ui->gain);
	}
	realloc_metrics(ui);
	prepare_metersurface(ui);
	return cb_reset_peak(NULL, NULL, handle);
}

/* val: 1 .. 1000 1/s  <> 0..100 */
#define ATTACKSCALE(X) ((X) > 0.1 ? rint(333.333 * (log10f(X)))/10.0 : 0)
#define INV_ATTACKSCALE(X) powf(10, (X) * .03f)

static gboolean set_attack(GtkWidget* w, gpointer handle) {
	SAUI* ui = (SAUI*)handle;
	if (!ui->disable_signals) {
		float val = INV_ATTACKSCALE(gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_attack)));
		//printf("set_attack %f -> %f\n", gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_attack)), val);
		ui->write(ui->controller, 36, sizeof(float), 0, (const void*) &val);
	}
	return TRUE;
}

/* val: .5 .. 15 1/s  <> 0..100 */
#define DECAYSCALE(X) ((X) > 0.01 ? rint(400.0 * (1.3f + log10f(X)) )/ 10.0 : 0)
#define INV_DECAYSCALE(X) powf(10, (X) * .025f - 1.3f)
static gboolean set_decay(GtkWidget* w, gpointer handle) {
	SAUI* ui = (SAUI*)handle;
	if (!ui->disable_signals) {
		float val = INV_DECAYSCALE(gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_decay)));
		//printf("set_decay %f -> %f\n", gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_decay)), val);
		ui->write(ui->controller, 37, sizeof(float), 0, (const void*) &val);
	}
	return TRUE;
}

static gboolean mousemove(GtkWidget *w, GdkEventMotion *event, gpointer handle) {
	SAUI* ui = (SAUI*)handle;
	if (event->y < GM_TOP || event->y > (GM_TOP + GM_SCALE)) {
		if (ui->highlight != -1) { gtk_widget_queue_draw(ui->m0); }
		ui->highlight = -1;
		return FALSE;
	}
	const int x = event->x - MA_WIDTH;
	const int remain =  x % ((int) GM_WIDTH);
	if (remain < GM_LEFT || remain > GM_LEFT + GM_GIRTH) {
		if (ui->highlight != -1) { gtk_widget_queue_draw(ui->m0); }
		ui->highlight = -1;
		return FALSE;
	}

	const int mtr = x / ((int) GM_WIDTH);
	if (mtr >=0 && mtr < ui->num_meters) {
		if (ui->highlight != mtr) { gtk_widget_queue_draw(ui->m0); }
		ui->highlight = mtr;
	} else {
		if (ui->highlight != -1) { gtk_widget_queue_draw(ui->m0); }
		ui->highlight = -1;
	}
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

	if      (!strcmp(plugin_uri, MTR_URI "spectrum")) { ui->num_meters = 31; ui->display_freq = true; }
	else if (!strcmp(plugin_uri, MTR_URI "dBTPmono")) { ui->num_meters = 1; ui->display_freq = false; }
	else if (!strcmp(plugin_uri, MTR_URI "dBTPstereo")) { ui->num_meters = 2; ui->display_freq = false; }
	else {
		free(ui);
		return NULL;
	}
	ui->write      = write_function;
	ui->controller = controller;

	ui->gain = 1.0;
	ui->cache_sf = -100;
	ui->cache_ma = -100;
	ui->highlight = -1;

	alloc_annotations(ui);
	realloc_metrics(ui);
	create_meter_pattern(ui);
	prepare_metersurface(ui);

	for (int i=0; i < ui->num_meters ; ++i) {
		ui->val[i] = -70.0;
		ui->peak_val[i] = -70.0;
	}

	ui->disable_signals = false;

	ui->box   = gtk_hbox_new(FALSE, 2);
	ui->c_box = gtk_vbox_new(FALSE, 2);
	ui->align = gtk_alignment_new(.5, .5, 0, 0);
	ui->m0    = gtk_drawing_area_new();
	ui->fader = gtk_vscale_new_with_range(1.0, 40.0, .001);

	ui->spn_attack = gtk_spin_button_new_with_range(0, 100, .5);
	ui->spn_decay  = gtk_spin_button_new_with_range(0, 100, .5);
	ui->lbl_attack = gtk_label_new("Attack:");
	ui->lbl_decay  = gtk_label_new("Decay:");

	/* fader init */
	gtk_scale_set_draw_value(GTK_SCALE(ui->fader), FALSE);
	gtk_range_set_value(GTK_RANGE(ui->fader), GAINSCALE(1.0000));
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(0.2511), GTK_POS_RIGHT, NULL /*"-12db" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(0.3548), GTK_POS_RIGHT, NULL /* "-9dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(0.5012), GTK_POS_RIGHT, NULL /* "-6dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(0.7079), GTK_POS_RIGHT, NULL /* "-3dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(1.0000), GTK_POS_RIGHT, "0dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(1.4125), GTK_POS_RIGHT, NULL /* "+3dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(1.9952), GTK_POS_RIGHT,  "+6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(2.8183), GTK_POS_RIGHT,  "+9dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(3.9810), GTK_POS_RIGHT, "+12dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(5.6234), GTK_POS_RIGHT, "+15dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(7.9432), GTK_POS_RIGHT, "+18dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader),GAINSCALE(10.0  ), GTK_POS_RIGHT, "+20dB");

	gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), 2.0 * MA_WIDTH + ui->num_meters * GM_WIDTH, GM_HEIGHT);
	gtk_widget_set_size_request(ui->m0, 2.0 * MA_WIDTH + ui->num_meters * GM_WIDTH, GM_HEIGHT);
	gtk_widget_set_redraw_on_allocate(ui->m0, TRUE);
	gtk_widget_set_size_request(ui->fader, -1, 200);

	/* layout */
	gtk_container_add(GTK_CONTAINER(ui->align), ui->m0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->align, TRUE, TRUE, 0);

	if (ui->display_freq) {
		gtk_box_pack_start(GTK_BOX(ui->c_box), ui->fader, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(ui->c_box), ui->lbl_attack, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(ui->c_box), ui->spn_attack, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(ui->c_box), ui->lbl_decay, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(ui->c_box), ui->spn_decay, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(ui->box), ui->c_box, TRUE, FALSE, 0);
	}

	gtk_widget_add_events(ui->m0, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);
	g_signal_connect (G_OBJECT (ui->m0), "button-release-event", G_CALLBACK (cb_reset_peak), ui);
	g_signal_connect (G_OBJECT (ui->fader), "value-changed", G_CALLBACK (set_gain), ui);
	g_signal_connect (G_OBJECT (ui->spn_attack), "value-changed", G_CALLBACK (set_attack), ui);
	g_signal_connect (G_OBJECT (ui->spn_decay), "value-changed", G_CALLBACK (set_decay), ui);
	if (ui->display_freq) {
		gtk_widget_add_events(ui->m0, GDK_POINTER_MOTION_MASK);
		g_signal_connect (G_OBJECT (ui->m0), "motion-notify-event", G_CALLBACK (mousemove), ui);
	}

	gtk_widget_show_all(ui->box);
	*widget = ui->box;

	if (!ui->display_freq) {
		/* dBTP run() re-sends peak-data */
		ui->initialized = false;
		float temp = -1;
		ui->write(ui->controller, 0, sizeof(float), 0, (const void*) &temp);
	} else {
		ui->initialized = true;
	}

	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	SAUI* ui = (SAUI*)handle;
	for (int i=0; i < ui->num_meters ; ++i) {
		cairo_surface_destroy(ui->sf[i]);
		cairo_surface_destroy(ui->an[i]);
	}
	cairo_pattern_destroy(ui->mpat);
	cairo_surface_destroy(ui->ma[0]);
	cairo_surface_destroy(ui->ma[1]);

	gtk_widget_destroy(ui->m0);
	gtk_widget_destroy(ui->fader);
	gtk_widget_destroy(ui->spn_attack);
	gtk_widget_destroy(ui->spn_decay);
	gtk_widget_destroy(ui->lbl_attack);
	gtk_widget_destroy(ui->lbl_decay);
	gtk_widget_destroy(ui->c_box);

	free(ui);
}

static void invalidate_meter(SAUI* ui, int mtr, float val, float peak) {
	const int old = deflect(ui, ui->val[mtr]);
	const int new = deflect(ui, val);
	const int m_old = deflect(ui, ui->peak_val[mtr]);
	const int m_new = deflect(ui, peak);

	int t, h;
	GdkRectangle rect;
	GdkRegion* region =  NULL;
	GdkRegion* tmp;

#define INVALIDATE_RECT(XX,YY,WW,HH) \
	rect.x=XX; rect.y=YY; rect.width=WW; rect.height=HH; \
	if (rect.x < 0 ) { rect.x = 0; } \
	if (rect.y < 0 ) { rect.y = 0; } \
	if (!region) { region =  gdk_region_rectangle (&rect); } \
	else { \
		tmp = gdk_region_rectangle (&rect); \
		gdk_region_union(region, tmp); \
		gdk_region_destroy(tmp); \
	}

	if (ui->val[mtr] != val && !ui->display_freq) {
		INVALIDATE_RECT(mtr * GM_WIDTH + MA_WIDTH, GM_TXT-12, GM_WIDTH, 18);
	}

	if (ui->highlight == mtr) {
		INVALIDATE_RECT(mtr * GM_WIDTH + MA_WIDTH + GM_WIDTH/2 - 32, GM_TXT - 10, 64, 54);
	}

	if (m_old != m_new && !ui->display_freq) {
		INVALIDATE_RECT(mtr * GM_WIDTH + MA_WIDTH, 0, GM_WIDTH, GM_TOP);
	}

	ui->val[mtr] = val;
	ui->peak_val[mtr] = peak;

	if (old != new) {
		if (old > new) {
			t = old;
			h = old - new;
		} else {
			t = new;
			h = new - old;
		}

		INVALIDATE_RECT(
				mtr * GM_WIDTH + MA_WIDTH + GM_LEFT - 1,
				GM_TOP + GM_SCALE - t - 1,
				GM_GIRTH + 2, h+3);
	}

	if (m_old != m_new) {
		if (m_old > m_new) {
			t = m_old;
			h = m_old - m_new;
		} else {
			t = m_new;
			h = m_new - m_old;
		}

		INVALIDATE_RECT(
				mtr * GM_WIDTH + MA_WIDTH + GM_LEFT - 1,
				GM_TOP + GM_SCALE - t - 1,
				GM_GIRTH + 2, h+4);
	}

	if (region) {
		gdk_window_invalidate_region (ui->m0->window, region, true);
	}
}

/******************************************************************************
 * handle data from backend
 */

static void handle_spectrum_connections(SAUI* ui, uint32_t port_index, float v) {
	if (port_index == 4) {
		if (v >= 0.25 && v <= 10.0) {
			ui->disable_signals = true;
			gtk_range_set_value(GTK_RANGE(ui->fader), GAINSCALE(v));
			ui->disable_signals = false;
		}
	} else
	if (port_index == 36) {
		ui->disable_signals = true;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_attack), ATTACKSCALE(v));
		ui->disable_signals = false;
	} else
	if (port_index == 37) {
		ui->disable_signals = true;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_decay), DECAYSCALE(v));
		ui->disable_signals = false;
	} else
	if (port_index > 4 && port_index < 5 + ui->num_meters) {
		int pidx = port_index -5;
		float np = ui->peak_val[pidx];
		if (v > np) { np = v; }
		invalidate_meter(ui, pidx, v, np);
	}
}

static void handle_meter_connections(SAUI* ui, uint32_t port_index, float v) {
	v = v > .000316f ? 20.0 * log10f(v) : -70.0;
	if (port_index == 3) {
		invalidate_meter(ui, 0, v, ui->peak_val[0]);
	}
	else if (port_index == 6) {
		invalidate_meter(ui, 1, v, ui->peak_val[1]);
	}

	/* peak data from backend */
	if (ui->num_meters == 1) {
		if (port_index == 4) {
			float np = ui->peak_val[0];
			if (v > np)  { np = v; }
			invalidate_meter(ui, 0, ui->val[0], np);
		}
	} else if (ui->num_meters == 2) {
		if (port_index == 7) {
			float np = ui->peak_val[0];
			if (v > np)  { np = v; }
			invalidate_meter(ui, 0, ui->val[0], np);
		}
		else if (port_index == 8) {
			float np = ui->peak_val[1];
			if (v > np)  { np = v; }
			invalidate_meter(ui, 1, ui->val[1], np);
		}
	}
}

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	SAUI* ui = (SAUI*)handle;
	if (format != 0) return;

	if (!ui->initialized && port_index != 0) {
		ui->initialized = true;
		float temp = -2;
		ui->write(ui->controller, 0, sizeof(float), 0, (const void*) &temp);
	}
	if (ui->display_freq) {
		handle_spectrum_connections(ui, port_index, *(float *)buffer);
	} else {
		handle_meter_connections(ui, port_index, *(float *)buffer);
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
	"http://gareus.org/oss/lv2/meters#dpmui",
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
