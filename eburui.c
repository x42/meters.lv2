/* ebu-r128 LV2 GUI
 *
 * Copyright 2012-2013 Robin Gareus <robin@gareus.org>
 * Copyright 2011-2012 David Robillard <d@drobilla.net>
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
#define MAX_CAIRO_PATH 32

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>
#include "common_cairo.h"

#include "gtkextrbtn.h"
#include "gtkextspin.h"
#include "gtkextpbtn.h"

#define GBT_W(PTR) gtkext_cbtn_widget(PTR)
#define GRB_W(PTR) gtkext_rbtn_widget(PTR)
#define GSP_W(PTR) gtkext_spin_widget(PTR)
#define GPB_W(PTR) gtkext_pbtn_widget(PTR)

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#include "./uris.h"

enum {
	FONT_M14 = 0,
	FONT_M12,
	FONT_M09,
	FONT_M08,
	FONT_S09,
	FONT_S08
};

typedef struct {
	LV2_Atom_Forge forge;

	LV2_URID_Map* map;
	EBULV2URIs   uris;

	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	GtkWidget* box;

	GtkExtCBtn* btn_start;
	GtkExtPBtn* btn_reset;

	GtkWidget* cbx_box;
	GtkExtRBtn* cbx_lufs;
	GtkExtRBtn* cbx_lu;
	GtkExtRBtn* cbx_sc9;
	GtkExtRBtn* cbx_sc18;
	GtkExtRBtn* cbx_sc24;
	GtkExtRBtn* cbx_ring_short;
	GtkExtRBtn* cbx_ring_mom;
	GtkExtRBtn* cbx_hist_short;
	GtkExtRBtn* cbx_hist_mom;
	GtkExtCBtn* cbx_transport;
	GtkExtCBtn* cbx_autoreset;
	GtkExtCBtn* cbx_truepeak;

	GtkExtRBtn* cbx_radar;
	GtkExtRBtn* cbx_histogram;

	GtkExtSpin* spn_radartime;
	GtkWidget* lbl_ringinfo;
	GtkWidget* lbl_radarinfo;
	GtkWidget* sep_v0;

	GtkWidget* m0;
	GtkWidget* align;
	cairo_pattern_t * cpattern;
	cairo_pattern_t * hpattern;
	GdkRegion * polygon_radar;
	GdkRegion * polygon_meter;

	bool fontcache;
	PangoFontDescription *font[6];

	bool disable_signals;

	/* current data */
	float lm, mm, ls, ms, il, rn, rx, it, tp;

	float *radarS;
	float *radarM;
	int radar_pos_cur;
	int radar_pos_max;

	int histS[HIST_LEN];
	int histM[HIST_LEN];
	int histLenS;
	int histLenM;

	/* displayed data */
	int radar_pos_disp;
	int circ_max;
	int circ_val;

} EBUrUI;

static inline float fast_log2 (float val)
{
	union {float f; int i;} t;
	t.f = val;
	int * const    exp_ptr =  &t.i;
	int            x = *exp_ptr;
	const int      log_2 = ((x >> 23) & 255) - 128;
	x &= ~(255 << 23);
	x += 127 << 23;
	*exp_ptr = x;

	val = ((-1.0f/3) * t.f + 2) * t.f - 2.0f/3;

	return (val + log_2);
}

static inline float fast_log10 (const float val)
{
	return fast_log2(val) * 0.301029996f;
}

static inline float coef_to_db (const float val) {
	if (val == 0) return -INFINITY;
#if 1
	return 20.0 * log10f(val);
#else
	return 20.0 * fast_log10(val);
#endif
}


static float radar_deflect(const float v, const float r) {
	if (v < -60) return 0;
	if (v > 0) return r;
	return (v + 60.0) * r / 60.0;
}

static void radar_color(cairo_t* cr, const float v, float alpha) {
	if (alpha > 0.9) alpha = 0.9;
	else if (alpha < 0) alpha = 1.0;

	if (v < -70) {
		cairo_set_source_rgba (cr, .3, .3, .3, alpha);
	} else if (v < -53) {
		cairo_set_source_rgba (cr, .0, .0, .5, alpha);
	} else if (v < -47) {
		cairo_set_source_rgba (cr, .0, .0, .9, alpha);
	} else if (v < -35) {
		cairo_set_source_rgba (cr, .0, .6, .0, alpha);
	} else if (v < -23) {
		cairo_set_source_rgba (cr, .0, .9, .0, alpha);
	} else if (v < -11) {
		cairo_set_source_rgba (cr, .75, .75, .0, alpha);
	} else if (v < -7) {
		cairo_set_source_rgba (cr, .8, .4, .0, alpha);
	} else if (v < -3.5) {
		cairo_set_source_rgba (cr, .75, .0, .0, alpha);
	} else {
		cairo_set_source_rgba (cr, 1.0, .0, .0, alpha);
	}
}

/******************************************************************************
 * custom visuals
 */

#define LUFS(V) ((V) < -100 ? -INFINITY : (lufs ? (V) : (V) + 23.0))
#define FONT(A) ui->font[(A)]


static void write_text(
		cairo_t* cr,
		const char *txt,
		PangoFontDescription *font, //const char *font,
		const float x, const float y,
		const float ang, const int align,
		const float * const col) {
	write_text_full(cr, txt, font, x, y, ang, align, col);
}


static cairo_pattern_t * radar_pattern(cairo_t* cr, float cx, float cy, float rad) {
	cairo_pattern_t * pat = cairo_pattern_create_radial(cx, cy, 0, cx, cy, rad);
	cairo_pattern_add_color_stop_rgba(pat, 0.0 ,  .0, .0, .0, 0.0);
	cairo_pattern_add_color_stop_rgba(pat, 0.10,  .0, .0, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-50, 1.0),  .0, .0, .5, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-47, 1.0),  .0, .0, .8, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-41, 1.0),  .0, .5, .2, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-31, 1.0),  .0, .7, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-25, 1.0),  .0, .9, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-22, 1.0), .75,.75, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect(-11.5, 1.0),  .8, .4, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, radar_deflect( -4, 1.0),  .8, .0, .0, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, 1.0 ,  .9, .0, .0, 1.0);

	return pat;
}

static cairo_pattern_t * histogram_pattern(cairo_t* cr, float cx, float cy, float rad) {
	cairo_pattern_t * pat = cairo_pattern_create_radial(cx, cy, 0, cx, cy, rad);
	cairo_pattern_add_color_stop_rgba(pat, 0.00,  .0, .0, .0, 0.0);
	cairo_pattern_add_color_stop_rgba(pat, 0.06,  .0, .0, .0, 0.0);
	cairo_pattern_add_color_stop_rgba(pat, 0.09,  .1, .1, .1, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, 0.20,  .5, .5, .6, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, 0.50,  .5, .6, .5, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, 0.90,  .6, .5, .5, 1.0);
	cairo_pattern_add_color_stop_rgba(pat, 1.0 , 1.0, .2, .2, 1.0);

	return pat;
}

static GdkRegion * make_polygon_radar() {
	const GdkPoint polygon_radar[12] = {
		{ 39, 190},
		{ 56, 126}, // 2
		{101,  82},
		{165,  63},
		{230,  81}, // 5
		{272, 126},
		{291, 190},
		{272, 255}, // 8
		{229, 298},
		{165, 315},
		{101, 298}, //10
		{ 56, 254}
	};
	return gdk_region_polygon(polygon_radar, 12, GDK_EVEN_ODD_RULE);
}

static GdkRegion * make_polygon_meter() {
	const GdkPoint polygon_meter[26] = {
		{ 37, 190}, // 1
		{ 55, 124},
		{ 98,  80},
		{165,  61},
		{231,  80}, // 5
		{276, 126},
		{292, 190},
		{292, 200}, // 7B

		{268, 268}, //8
		{315, 268}, //8A

		{330, 200}, // 7A
		{330, 185}, // 7C
		{310, 112}, // 6A
		{242,  47}, // 5A
		{165,  33}, // 4A
		{ 87,  47}, // 3A
		{ 20, 113}, // 2A
		{  0, 190}, // 1A
		{ 25, 267}, //12A
		{ 80, 332}, //11A
		{165, 345}, //10A
		{180, 345}, //10B
		{180, 315}, //10B

		{160, 316}, //10
		{101, 301}, //11
		{ 54, 254}  //12
	};
	return gdk_region_polygon(polygon_meter, 26, GDK_EVEN_ODD_RULE);
}

static int check_overlap(EBUrUI* ui, const GdkRegion *r) {
	int rv = 0;
	GdkRegion* rr;

	rr = gdk_region_copy (ui->polygon_meter);
	gdk_region_intersect(rr, r);
	rv |= gdk_region_empty (rr) ? 0 : 1;
	gdk_region_destroy(rr);

	rr = gdk_region_copy (ui->polygon_radar);
	gdk_region_intersect(rr, r);
	rv |= gdk_region_empty (rr) ? 0 : 2;
	gdk_region_destroy(rr);
	return rv;
}

static void ring_leds(EBUrUI* ui, int *l, int *m) {
	const bool rings = gtkext_rbtn_get_active(ui->cbx_ring_short);
	const bool plus9 = gtkext_rbtn_get_active(ui->cbx_sc9);

	const float clr = rings ? ui->ls : ui->lm;
	const float cmr = rings ? ui->ms : ui->mm;
	*l = rint(plus9 ? (clr + 41.0f) * 4 : (clr + 59.0f) * 2.0);
	*m = rint(plus9 ? (cmr + 41.0f) * 4 : (cmr + 59.0f) * 2.0);
}

static void initialize_font_cache(EBUrUI* ui) {
	ui->fontcache = true;
	ui->font[FONT_M14] = pango_font_description_from_string("Mono 14");
	ui->font[FONT_M12] = pango_font_description_from_string("Mono 12");
	ui->font[FONT_M09] = pango_font_description_from_string("Mono 9");
	ui->font[FONT_M08] = pango_font_description_from_string("Mono 8");
	ui->font[FONT_S09] = pango_font_description_from_string("Sans 9");
	ui->font[FONT_S08] = pango_font_description_from_string("Sans 8");
}

static gboolean expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	const bool lufs =  gtkext_rbtn_get_active(ui->cbx_lufs);
	const bool rings = gtkext_rbtn_get_active(ui->cbx_ring_short);
	const bool hists = gtkext_rbtn_get_active(ui->cbx_hist_short);
	const bool plus9 = gtkext_rbtn_get_active(ui->cbx_sc9);
	const bool plus24= gtkext_rbtn_get_active(ui->cbx_sc24);
	const bool dbtp  = gtkext_cbtn_get_active(ui->cbx_truepeak);

	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));

	char buf[128];
	int redraw_part = 0;

#define CX  (165.5f)
#define CY  (190.5f)
#define RADIUS   (120.0f)
#define RADIUS5  (125.0f)
#define RADIUS10 (130.0f)
#define RADIUS19 (139.0f)
#define RADIUS22 (142.0f)
#define RADIUS23 (143.0f)

	if (!ui->cpattern) {
		ui->cpattern = radar_pattern(cr, CX, CY, RADIUS);
	}
	if (!ui->hpattern) {
		ui->hpattern = histogram_pattern(cr, CX, CY, RADIUS);
	}

#if 0 // DEBUG
	printf("IS: %dx%d+%d+%d %d\n", ev->area.x, ev->area.y, ev->area.width, ev->area.height,
			check_overlap(ev->region));
#endif

	if (ev->area.x == 0 && ev->area.y == 0 && ev->area.width == 330 && ev->area.height == 400) {
		redraw_part = 3;
	} else {
		redraw_part = check_overlap(ui, ev->region);
		cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
		cairo_clip (cr);
	}

	/* fill background */
	cairo_rectangle (cr, 0, 0, 330, 400);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_fill (cr);
	write_text(cr, "EBU R128 LV2", FONT(FONT_S08), 2 , 5, 1.5 * M_PI, 7, c_gry);

	/* big level as text */
	sprintf(buf, "%+5.1f %s", LUFS( rings ? ui->ls : ui->lm), lufs ? "LUFS" : "LU");
	write_text(cr, buf, FONT(FONT_M14), CX , 10, 0, 8, c_wht);

	/* max level background */
	int trw = lufs ? 87 : 75;
	cairo_set_source_rgba (cr, .1, .1, .1, 1.0);
	rounded_rectangle (cr, 275, 30, 40, 30, 10);
	cairo_fill (cr);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	rounded_rectangle (cr, 325-trw, 5, trw, 38, 10);
	cairo_fill (cr);

	/* display max level as text */
	write_text(cr, !rings ? "Mom":"Short", FONT(FONT_S08), 295, 45, 0, 8, c_wht);
	sprintf(buf, "Max:\n%+5.1f %s", LUFS( rings ? ui->ms: ui->mm), lufs ? "LUFS" : "LU");
	write_text(cr, buf, FONT(FONT_M09), 315, 10, 0, 7, c_wht);

	if (dbtp) {
		/* true peak level */
		cairo_set_source_rgba (cr, .1, .1, .1, 1.0);
		rounded_rectangle (cr, 35, 30, 40, 30, 10);
		cairo_fill (cr);
		if (ui->tp >= 0.8912f) { // -1dBFS
			cairo_set_source_rgba (cr, .8, .2, .2, 1.0);
		} else {
			cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
		}
		rounded_rectangle (cr, 25, 5, 75, 38, 10);
		cairo_fill (cr);

		/* true-peak val */
		sprintf(buf, "%+5.1f", coef_to_db(ui->tp));
		write_text(cr, buf, FONT(FONT_M09), 90, 10, 0, 7, c_wht);
		write_text(cr, "dBTP", FONT(FONT_M09), 90, 24, 0, 7, c_wht);
		write_text(cr, "True", FONT(FONT_S08), 55, 45, 0, 8, c_wht);
	}

#if 1 /* Radar */

	if (gtkext_rbtn_get_active(ui->cbx_histogram) && (redraw_part & 2) == 2) {
		/* ----- Histogram ----- */
		const int *rdr = hists ? ui->histS : ui->histM;
		const int  len = hists ? ui->histLenS : ui->histLenM;

		/* histogram background */
		if (len > 0) {
			cairo_set_source_rgba (cr, .05, .05, .05, 1.0);
		} else {
			cairo_set_source_rgba (cr, .25, .00, .00, 1.0);
		}
		cairo_arc (cr, CX, CY, RADIUS, 0, 2.0 * M_PI);
		cairo_fill (cr);

		if (len > 0) {
			int amin, amax;
			//  lvlFS = (0.1f * (ang - 700))
			//  lvlFS =  .1 * ang - 70
			if (plus9) { // -41 .. -14 LUFS
				amin = 290; // -41LUFS
				amax = 560; // -14LUFS
			} else { // -59 .. -5 LUFS
				amin = 110; // -59LUFS
				amax = 650; // -5LUFS
			}
			const double astep = 1.5 * M_PI / (double) (amax - amin);
			const double aoff = (M_PI / 2.0) - amin * astep;

			cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
			cairo_set_line_width(cr, 0.75);
			cairo_set_source (cr, ui->hpattern);

			int cnt = 0;
			for (int ang = amin; ang < amax; ++ang) {
				if (rdr[ang] <= 0) continue;
				const float rad = (float) RADIUS * (1.0 + fast_log10(rdr[ang] / (float) len));
				if (rad < 5) continue;

				cairo_move_to(cr, CX, CY);
				cairo_arc (cr, CX, CY, rad,
						(double) ang * astep + aoff, (ang+1.0) * astep + aoff);
				cairo_line_to(cr, CX, CY);

				if (++cnt > MAX_CAIRO_PATH) {
					cnt = 0;
					cairo_stroke_preserve(cr);
					cairo_fill(cr);
				}
			}
			if (cnt > 0) {
				cairo_stroke_preserve(cr);
				cairo_fill(cr);
			}

			cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

			/* outer circle */
			cairo_set_line_width(cr, 1.0);
			cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
			cairo_arc (cr, CX, CY, RADIUS, 0.5 * M_PI, 2.0 * M_PI);
			cairo_stroke (cr);

			cairo_set_source_rgba (cr, .5, .5, .5, 0.5);

#define CIRCLABEL(RDS,LBL) \
	{ \
	cairo_arc (cr, CX, CY, RADIUS * RDS, 0.5 * M_PI, 2.0 * M_PI); \
	cairo_stroke (cr); \
	write_text(cr, LBL, FONT(FONT_M08), CX + RADIUS * RDS, CY + 14, M_PI * -.5, 2, c_gry);\
	}
			// POS = fast_log10(VAL) + 1;
			CIRCLABEL(.301, "20%")
			CIRCLABEL(.602, "40%")
			CIRCLABEL(.903, "80%")
		} else {
			write_text(cr, "No integration\ndata available.", FONT(FONT_S08), CX + RADIUS / 2, CY + 5, 0, 8, c_gry);
		}

		/* center circle */
		const float innercircle = 6;
		cairo_set_line_width(cr, 1.0);
		cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
		cairo_arc (cr, CX, CY, innercircle, 0, 2.0 * M_PI);
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, .5, .5, .5, 0.5);
		cairo_stroke(cr);

		cairo_arc (cr, CX, CY, innercircle + 3, .5 * M_PI, 2.0 * M_PI);
		cairo_stroke(cr);

		/* gain lines */
		const double dashed[] = {3.0, 5.0};
		cairo_save(cr);
		cairo_set_dash(cr, dashed, 2, 4.0);
		cairo_set_line_width(cr, 1.5);
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
		for (int i = 3; i <= 12; ++i) {
			const float ang = .5235994f * i;
			float cc = sinf(ang);
			float sc = cosf(ang);
			cairo_move_to(cr, CX + innercircle * sc, CY + innercircle * cc);
			cairo_line_to(cr, CX + RADIUS5 * sc, CY + RADIUS5 * cc);
			cairo_stroke (cr);
		}
		cairo_restore(cr);

	} else if (redraw_part & 2) {
		/* ----- History ----- */
			ui->radar_pos_disp = ui->radar_pos_cur;

		/* radar background */
		cairo_set_source_rgba (cr, .05, .05, .05, 1.0);
		cairo_arc (cr, CX, CY, RADIUS, 0, 2.0 * M_PI);
		cairo_fill (cr);

		cairo_set_line_width(cr, 1.0);
		cairo_set_source (cr, ui->cpattern);
		int cnt = 0;
		if (ui->radar_pos_max > 0) {
			float *rdr = hists ? ui->radarS : ui->radarM;
			const double astep = 2.0 * M_PI / (double) ui->radar_pos_max;

			for (int ang = 0; ang < ui->radar_pos_max; ++ang) {
				cairo_move_to(cr, CX, CY);
				cairo_arc (cr, CX, CY, radar_deflect(rdr[ang], RADIUS),
						(double) ang * astep, (ang+1.0) * astep);
				cairo_line_to(cr, CX, CY);
				if (++cnt > MAX_CAIRO_PATH) {
					cnt = 0;
					cairo_fill(cr);
				}
			}
			if (cnt > 0) {
				cairo_fill(cr);
			}

			/* shade */
			for (int p = 0; p < 12; ++p) {
				float pos = ui->radar_pos_cur + 1 + p;
				cairo_set_source_rgba (cr, .0, .0, .0, 1.0 - ((p+1.0)/12.0));
				cairo_move_to(cr, CX, CY);
				cairo_arc (cr, CX, CY, RADIUS,
							pos * astep, (pos + 1.0) * astep);
				cairo_fill(cr);
			}

			/* current position */
			cairo_set_source_rgba (cr, .7, .7, .7, 0.3);
			cairo_move_to(cr, CX, CY);
			cairo_arc (cr, CX, CY, RADIUS,
						(double) ui->radar_pos_cur * astep, ((double) ui->radar_pos_cur + 1.0) * astep);
			cairo_line_to(cr, CX, CY);
			cairo_stroke (cr);
		}

		/* radar lines */
		cairo_set_line_width(cr, 1.5);
		cairo_set_source_rgba (cr, .6, .6, .9, 0.75);
		cairo_arc (cr, CX, CY, radar_deflect(-23, RADIUS), 0, 2.0 * M_PI);
		cairo_stroke (cr);

		cairo_set_line_width(cr, 1.0);
		cairo_set_source_rgba (cr, .5, .5, .8, 0.75);
		cairo_arc (cr, CX, CY, radar_deflect(-47, RADIUS), 0, 2.0 * M_PI);
		cairo_stroke (cr);
		cairo_arc (cr, CX, CY, radar_deflect(-35, RADIUS), 0, 2.0 * M_PI);
		cairo_stroke (cr);
		cairo_arc (cr, CX, CY, radar_deflect(-11, RADIUS), 0, 2.0 * M_PI);
		cairo_stroke (cr);
		cairo_arc (cr, CX, CY, radar_deflect( 0, RADIUS), 0, 2.0 * M_PI);
		cairo_stroke (cr);

		const float innercircle = radar_deflect(-47, RADIUS);
		for (int i = 0; i < 12; ++i) {
			const float ang = .5235994f * i;
			float cc = sinf(ang);
			float sc = cosf(ang);
			cairo_move_to(cr, CX + innercircle * sc, CY + innercircle * cc);
			cairo_line_to(cr, CX + RADIUS * sc, CY + RADIUS * cc);
			cairo_stroke (cr);
		}
	}
#endif

#if 1 /* circular Level display */
	if (redraw_part & 1) {
		int cl, cm;
		ring_leds(ui, &cl, &cm);

		ui->circ_max = cm;
		ui->circ_val = cl;

		cairo_set_line_width(cr, 2.5);
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

		bool maxed = false; // peak
		int ulp = plus24 ? 120 : 108;

		for (int rng = 0; rng <= ulp; ++rng) {
			const float ang = 0.043633231 * rng + 1.570796327;
			float val;
			if (plus9) {
				val = (float) rng * .25 - 41.0f;
			} else {
				val = (float) rng * .5 - 59.0f;
			}
			if (rng <= cl) {
				radar_color(cr, val, -1);
			} else {
				cairo_set_source_rgba (cr, .3, .3, .3, 1.0);
			}
#if 0
			cairo_save(cr);
			cairo_translate (cr, CX, CY);
			cairo_rotate (cr, ang);
			cairo_translate (cr, RADIUS10, 0);
			cairo_move_to(cr,  0.5, 0);
			if (!maxed && cm > 0 && (rng >= cm || (rng == ulp && cm >= ulp))) {
				radar_color(cr, val, -1);
				cairo_line_to(cr, 12.5, 0);
				maxed = true;
			} else {
				cairo_line_to(cr,  9.5, 0);
			}
			cairo_stroke (cr);
			cairo_restore(cr);
#else
			float cc = sinf(ang);
			float sc = cosf(ang);
			cairo_move_to(cr,   CX + RADIUS10 * sc, CY + RADIUS10 * cc);

			/* highligh peak */
			if (!maxed && cm > 0 && (rng >= cm || (rng == ulp && cm >= ulp))) {
				radar_color(cr, val, -1);
				cairo_line_to(cr, CX + RADIUS22 * sc, CY + RADIUS22 * cc);
				maxed = true;
			} else {
				cairo_line_to(cr, CX + RADIUS19 * sc, CY + RADIUS19 * cc);
			}
			cairo_stroke (cr);
#endif
		}

#define SIN60 0.866025404
#define CLABEL(PT, XS, YS, AL) \
		sprintf(buf, "%+.0f", LUFS(PT)); \
		write_text(cr, buf, FONT(FONT_M08), CX + RADIUS23 * XS, CY + RADIUS23 *YS , 0, AL, c_gry);

		if (plus9) {
			CLABEL(-41,    0.0,   1.0, 8)
			CLABEL(-38,   -0.5, SIN60, 7)
			CLABEL(-35, -SIN60,   0.5, 1)
			CLABEL(-32,   -1.0,   0.0, 1)
			CLABEL(-29, -SIN60,  -0.5, 1)
			CLABEL(-26,   -0.5,-SIN60, 4)
			CLABEL(-23,    0.0,  -1.0, 5)
			CLABEL(-20,    0.5,-SIN60, 6)
			CLABEL(-17,  SIN60,  -0.5, 3)
			CLABEL(-14,    1.0,   0.0, 3)
		} else {
			CLABEL(-59,    0.0,   1.0, 8)
			CLABEL(-53,   -0.5, SIN60, 7)
			CLABEL(-47, -SIN60,   0.5, 1)
			CLABEL(-41,   -1.0,   0.0, 1)
			CLABEL(-35, -SIN60,  -0.5, 1)
			CLABEL(-29,   -0.5,-SIN60, 4)
			CLABEL(-23,    0.0,  -1.0, 5)
			CLABEL(-17,    0.5,-SIN60, 6)
			CLABEL(-11,  SIN60,  -0.5, 3)
			CLABEL( -5,    1.0,   0.0, 3)
			if (plus24) {
				CLABEL(1,  SIN60,   0.5, 3)
			}
		}
	}
#endif

	int bottom_max_offset = 50;

	/* integrated level text display */
	if (ui->il > -60 || gtkext_cbtn_get_active(ui->btn_start)) {
		bottom_max_offset = 3;

		cairo_set_source_rgba (cr, .1, .1, .1, 1.0);
		rounded_rectangle (cr, 15, 335, 40, 30, 10);
		cairo_fill (cr);
		write_text(cr, "Long", FONT(FONT_S08), 35 , 353, 0,  5, c_wht);

		cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
		rounded_rectangle (cr, 5, 355, 320, 40, 10);
		cairo_fill (cr);

		if (ui->il > -60) {
			sprintf(buf, "Int:   %+5.1f %s", LUFS(ui->il), lufs ? "LUFS" : "LU");
			write_text(cr, buf, FONT(FONT_M09), 15 , 375, 0,  6, c_wht);
		} else {
			sprintf(buf, "[Integrating over 5 sec]");
			write_text(cr, buf, FONT(FONT_S09), 15 , 375, 0,  6, c_wht);
		}

		if (ui->rx > -60.0 && ui->rn > -60.0) {
			sprintf(buf, "Range: %+5.1f..%+5.1f %s (%4.1f)",
					LUFS(ui->rn), LUFS(ui->rx), lufs ? "LUFS" : "LU", (ui->rx - ui->rn));
			write_text(cr, buf, FONT(FONT_M09), 15 , 390, 0,  6, c_wht);
		} else {
			sprintf(buf, "[Collecting 10 sec range.]");
			write_text(cr, buf, FONT(FONT_S09), 15 , 390, 0,  6, c_wht);
		}

		/* clock */
		if (ui->it < 60) {
			sprintf(buf, "%.1f\"", ui->it);
		} else if (ui->it < 600) {
			int minutes = ui->it / 60;
			int seconds = ((int)floorf(ui->it)) % 60;
			int ds = 10*(ui->it - seconds - 60*minutes);
			sprintf(buf, "%d'%02d\"%d", minutes, seconds, ds);
		} else if (ui->it < 3600) {
			int minutes = ui->it / 60;
			int seconds = ((int)floorf(ui->it)) % 60;
			sprintf(buf, "%d'%02d\"", minutes, seconds);
		} else {
			int hours = ui->it / 3600;
			int minutes = ((int)floorf(ui->it / 60)) % 60;
			sprintf(buf, "%dh%02d'", hours, minutes);
		}
		write_text(cr, buf, FONT(FONT_M12), 318, 385, 0,  4, c_wht);

	}

	/* bottom level text display */
	trw = lufs ? 117 : 105;
	cairo_set_source_rgba (cr, .1, .1, .1, 1.0);
	rounded_rectangle (cr, 275, 285+bottom_max_offset, 40, 30, 10);
	cairo_fill (cr);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	rounded_rectangle (cr, 325-trw, 305+bottom_max_offset, trw, 40, 10);
	cairo_fill (cr);

	write_text(cr, rings ? "Mom":"Short", FONT(FONT_S08), 295, 290+bottom_max_offset, 0, 8, c_wht);
	sprintf(buf, "%+5.1f %s", LUFS(!rings ? ui->ls : ui->lm), lufs ? "LUFS" : "LU");
	write_text(cr, buf, FONT(FONT_M09), 315, 310+bottom_max_offset, 0, 7, c_wht);
	sprintf(buf, "Max:%+5.1f %s", LUFS(!rings ? ui->ms: ui->mm), lufs ? "LUFS" : "LU");
	write_text(cr, buf, FONT(FONT_M09), 315, 325+bottom_max_offset, 0, 7, c_wht);

	cairo_destroy (cr);
	return TRUE;
}

static void invalidate_changed(EBUrUI* ui, int what) {
	GdkRectangle rect;
	GdkRegion *tmp = 0;

	if (what == -1) {
		gtk_widget_queue_draw(ui->m0);
		return;
	}

	/* initialize using top gain display */
	rect.x=105; rect.y=5;
	rect.width=120; rect.height=24;
	GdkRegion* region =  gdk_region_rectangle (&rect);

#define INVALIDATE_RECT(XX,YY,WW,HH) \
	rect.x=XX; rect.y=YY; rect.width=WW; rect.height=HH; \
	tmp = gdk_region_rectangle (&rect); \
	gdk_region_union(region, tmp); \
	gdk_region_destroy(tmp);

	if (gtkext_cbtn_get_active(ui->cbx_truepeak)) {
		INVALIDATE_RECT(25, 5, 75, 38)    // top left side
		INVALIDATE_RECT(35, 30, 40, 30)   // top left side tab
	}

	INVALIDATE_RECT(243, 5, 87, 38)    // top side
	INVALIDATE_RECT(275, 30, 30, 30)   // top side tab

	INVALIDATE_RECT(0, 355, 330, 45)   // bottom bar
	INVALIDATE_RECT(15, 335, 40, 30)   // bottom bar left tab
	INVALIDATE_RECT(208, 335, 117, 40) // bottom bar right space

	INVALIDATE_RECT(208, 308, 117, 40) // bottom side
	INVALIDATE_RECT(275, 287, 40, 30)  // bottom side tab

	if ((what & 1) ||
			(gtkext_rbtn_get_active(ui->cbx_radar)
			 && ui->radar_pos_cur != ui->radar_pos_disp)) {

		GdkRegion* rr = gdk_region_copy(ui->polygon_radar);

#define MIN2(A,B) ( (A) < (B) ? (A) : (B) )
#define MAX2(A,B) ( (A) > (B) ? (A) : (B) )
#define MIN3(A,B,C) (  (A) < (B)  ? MIN2 (A,C) : MIN2 (B,C) )
#define MAX3(A,B,C) (  (A) > (B)  ? MAX2 (A,C) : MAX2 (B,C) )

#if 1 /* invalidate changed part of radar only */
		if ((what & 2) == 0 && ui->radar_pos_max > 0) {
			float ang0 = 2.0 * M_PI * (ui->radar_pos_cur - 1) / (float) ui->radar_pos_max;
			int dx0 = rintf(CX + RADIUS5 * cosf(ang0));
			int dy0 = rintf(CY + RADIUS5 * sinf(ang0));

			float ang1 = 2.0 * M_PI * (ui->radar_pos_cur + 13) / (float) ui->radar_pos_max;
			int dx1 = rint(CX + RADIUS5 * cosf(ang1));
			int dy1 = rint(CY + RADIUS5 * sinf(ang1));

			rect.x = MIN3(CX, dx0, dx1) -1;
			rect.y = MIN3(CY, dy0, dy1) -1;

			rect.width  = 2 + MAX3(CX, dx0, dx1) - rect.x;
			rect.height = 2 + MAX3(CY, dy0, dy1) - rect.y;

			tmp = gdk_region_rectangle (&rect);
			gdk_region_intersect(rr, tmp);
			gdk_region_destroy(tmp);
		}
#endif

		gdk_region_union(region, rr);
		gdk_region_destroy(rr);
	}

	int cl, cm;
	ring_leds(ui, &cl, &cm);

	if (ui->circ_max != cm || ui->circ_val != cl) {
		gdk_region_union(region, ui->polygon_meter);
	}

	gdk_window_invalidate_region (ui->m0->window, region, true);
}

static void invalidate_histogram_line(EBUrUI* ui, int p) {
	const bool plus9 = gtkext_rbtn_get_active(ui->cbx_sc9);
	GdkRectangle rect;
	GdkRegion *tmp = 0;

	// dup from expose_event()
	int amin, amax;
	if (plus9) {
		amin = 290;
		amax = 560;
	} else {
		amin = 110;
		amax = 650;
	}
	if (p < amin || p > amax) return;
	const double astep = 1.5 * M_PI / (double) (amax - amin);
	const double aoff = (M_PI / 2.0) - amin * astep;

	float ang0 = (float) (p-1) * astep + aoff;
	float ang1 = (float) (p+1) * astep + aoff;

	// see also "invalidate changed part of radar only" above
	int dx0 = rintf(CX + RADIUS5 * cosf(ang0));
	int dy0 = rintf(CY + RADIUS5 * sinf(ang0));
	int dx1 = rint(CX + RADIUS5 * cosf(ang1));
	int dy1 = rint(CY + RADIUS5 * sinf(ang1));

	rect.x = MIN3(CX, dx0, dx1) -1;
	rect.y = MIN3(CY, dy0, dy1) -1;

	rect.width  = 2 + MAX3(CX, dx0, dx1) - rect.x;
	rect.height = 2 + MAX3(CY, dy0, dy1) - rect.y;

	tmp = gdk_region_rectangle (&rect);
	gdk_window_invalidate_region (ui->m0->window, tmp, true);
	gdk_region_destroy(tmp);
}


/******************************************************************************
 * LV2 UI -> plugin communication
 */

static void forge_message_kv(EBUrUI* ui, LV2_URID uri, int key, float value) {
  uint8_t obj_buf[1024];
	if (ui->disable_signals) return;

  lv2_atom_forge_set_buffer(&ui->forge, obj_buf, 1024);
	LV2_Atom* msg = forge_kvcontrolmessage(&ui->forge, &ui->uris, uri, key, value);
  ui->write(ui->controller, 0, lv2_atom_total_size(msg), ui->uris.atom_eventTransfer, msg);
}

/******************************************************************************
 * UI callbacks
 */

static gboolean btn_start(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	if (gtkext_cbtn_get_active(ui->btn_start)) {
		// TODO
		//gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(ui->btn_start), GTK_STOCK_MEDIA_PAUSE);
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_START, 0);
	} else {
		// TODO
		//gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(ui->btn_start), GTK_STOCK_MEDIA_PLAY);
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_PAUSE, 0);
	}
	invalidate_changed(ui, -1);
	return TRUE;
}

static gboolean btn_reset(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
  forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_RESET, 0);
	return TRUE;
}

static gboolean cbx_transport(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	if (gtkext_cbtn_get_active(ui->cbx_transport)) {
		gtkext_cbtn_set_sensitive(ui->btn_start, false);
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_TRANSPORTSYNC, 1);
	} else {
		gtkext_cbtn_set_sensitive(ui->btn_start, true);
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_TRANSPORTSYNC, 0);
	}
	return TRUE;
}

static gboolean cbx_autoreset(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	if (gtkext_cbtn_get_active(ui->cbx_autoreset)) {
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_AUTORESET, 1);
	} else {
		forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_AUTORESET, 0);
	}
	return TRUE;
}

static gboolean cbx_lufs(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	uint32_t v = 0;
	v |= gtkext_rbtn_get_active(ui->cbx_lufs) ? 1 : 0;
	v |= gtkext_rbtn_get_active(ui->cbx_sc9) ? 2 : 0;
	v |= gtkext_rbtn_get_active(ui->cbx_sc24) ? 32 : 0;
	v |= gtkext_rbtn_get_active(ui->cbx_ring_short) ? 4 : 0;
	v |= gtkext_rbtn_get_active(ui->cbx_hist_short) ? 8 : 0;
	v |= gtkext_rbtn_get_active(ui->cbx_histogram) ? 16 : 0;
	v |= gtkext_cbtn_get_active(ui->cbx_truepeak) ? 64 : 0;
	forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_UISETTINGS, (float)v);
	invalidate_changed(ui, -1);
	return TRUE;
}

static gboolean spn_radartime(GtkWidget *w, gpointer handle) {
	EBUrUI* ui = (EBUrUI*)handle;
	 float v = gtkext_spin_get_value(ui->spn_radartime);
	forge_message_kv(ui, ui->uris.mtr_meters_cfg, CTL_RADARTIME, v);
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
	EBUrUI* ui = (EBUrUI*)calloc(1,sizeof(EBUrUI));
	ui->write      = write_function;
	ui->controller = controller;

	*widget = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID_URI "#map")) {
			ui->map = (LV2_URID_Map*)features[i]->data;
		}
	}

	if (!ui->map) {
		fprintf(stderr, "UI: Host does not support urid:map\n");
		free(ui);
		return NULL;
	}

	map_eburlv2_uris(ui->map, &ui->uris);

	lv2_atom_forge_init(&ui->forge, ui->map);

	ui->box = gtk_vbox_new(FALSE, 2);

	ui->m0    = gtk_drawing_area_new();
	ui->align = gtk_alignment_new(.5, .5, 0, 0);
	gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), 330, 400);
	gtk_widget_set_size_request(ui->m0, 330, 400);
	gtk_widget_set_redraw_on_allocate(ui->m0, TRUE);

	ui->btn_start = gtkext_cbtn_new("Integrate", GBT_LED_OFF, false);
	ui->btn_reset = gtkext_pbtn_new("Reset");

	ui->cbx_box = gtk_table_new(/*rows*/5, /*cols*/ 5, FALSE);
	ui->cbx_lu         = gtkext_rbtn_new("LU", NULL);
	ui->cbx_lufs       = gtkext_rbtn_new("LUFS", gtkext_rbtn_group(ui->cbx_lu));

	ui->cbx_ring_mom   = gtkext_rbtn_new("Momentary", NULL);
	ui->cbx_ring_short = gtkext_rbtn_new("Short", gtkext_rbtn_group(ui->cbx_ring_mom));

	ui->cbx_hist_short = gtkext_rbtn_new("Short", NULL);
	ui->cbx_hist_mom   = gtkext_rbtn_new("Momentary", gtkext_rbtn_group(ui->cbx_hist_short));

	ui->cbx_sc18       = gtkext_rbtn_new("-36..+18LU", NULL);
	ui->cbx_sc9        = gtkext_rbtn_new("-18..+9LU", gtkext_rbtn_group(ui->cbx_sc18));
	ui->cbx_sc24       = gtkext_rbtn_new("-36..+24LU", gtkext_rbtn_group(ui->cbx_sc18));

	ui->cbx_transport  = gtkext_cbtn_new("Host Transport", GBT_LED_LEFT, true);
	ui->cbx_autoreset  = gtkext_cbtn_new("Reset on Start", GBT_LED_LEFT, true);
	ui->spn_radartime  = gtkext_spin_new(30, 600, 15);
	ui->lbl_radarinfo  = gtk_label_new("History Length [s]:");
	ui->lbl_ringinfo   = gtk_label_new("Level Diplay");
	ui->cbx_truepeak   = gtkext_cbtn_new("Compute True-Peak", GBT_LED_LEFT, true);
	ui->sep_v0         = gtk_vseparator_new();

	ui->cbx_radar      = gtkext_rbtn_new("History", NULL);
	ui->cbx_histogram  = gtkext_rbtn_new("Histogram", gtkext_rbtn_group(ui->cbx_radar));

	gtk_misc_set_alignment(GTK_MISC(ui->lbl_radarinfo), 0.0f, 0.5f);

	gtk_table_attach(GTK_TABLE(ui->cbx_box), ui->lbl_ringinfo, 0, 2, 0, 1, GTK_SHRINK, GTK_SHRINK, 3, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_lu)   , 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_lufs) , 1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_sc18) , 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_sc9)  , 1, 2, 2, 3);
#ifdef EASTER_EGG
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_sc24)      , 1, 2, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GBT_W(ui->cbx_truepeak)  , 0, 1, 3, 4);
#else
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GBT_W(ui->cbx_truepeak)  , 0, 2, 3, 4);
#endif
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_ring_mom)  , 0, 1, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_ring_short), 1, 2, 4, 5);

	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), ui->sep_v0, 2, 3, 0, 5);

	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_radar)     , 4, 5, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_histogram) , 3, 4, 0, 1);

	gtk_table_attach(GTK_TABLE(ui->cbx_box), ui->lbl_radarinfo, 3, 4, 1, 2, GTK_FILL, GTK_SHRINK, 3, 0);
	gtk_table_attach(GTK_TABLE(ui->cbx_box), GSP_W(ui->spn_radartime), 4, 5, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 0);

	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GBT_W(ui->cbx_autoreset), 3, 4, 2, 3);
	gtk_table_attach(GTK_TABLE(ui->cbx_box), GPB_W(ui->btn_reset), 4, 5, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND|GTK_FILL, 3, 1);

	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GBT_W(ui->cbx_transport), 3, 4, 3, 4);
	gtk_table_attach(GTK_TABLE(ui->cbx_box), GBT_W(ui->btn_start), 4, 5, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND|GTK_FILL, 3, 1);

	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_hist_mom)  , 3, 4, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(ui->cbx_box), GRB_W(ui->cbx_hist_short), 4, 5, 4, 5);


	/* global packing */
	gtk_container_add(GTK_CONTAINER(ui->align), ui->m0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->align, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->cbx_box, FALSE, FALSE, 2);

	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);

	gtkext_cbtn_set_callback(ui->btn_start, btn_start, ui);
	gtkext_pbtn_set_callback(ui->btn_reset, btn_reset, ui);

	gtkext_spin_set_callback(ui->spn_radartime, spn_radartime, ui);

	gtkext_rbtn_set_callback(ui->cbx_lufs, cbx_lufs, ui);
	gtkext_rbtn_set_callback(ui->cbx_sc18, cbx_lufs, ui);

	gtkext_rbtn_set_callback(ui->cbx_hist_short, cbx_lufs, ui);
	gtkext_rbtn_set_callback(ui->cbx_ring_short, cbx_lufs, ui);
	gtkext_rbtn_set_callback(ui->cbx_histogram, cbx_lufs, ui);
	gtkext_cbtn_set_callback(ui->cbx_truepeak, cbx_lufs, ui);

	gtkext_cbtn_set_callback(ui->cbx_transport, cbx_transport, ui);
	gtkext_cbtn_set_callback(ui->cbx_autoreset, cbx_autoreset, ui);


	gtk_widget_show_all(ui->box);
	*widget = ui->box;

	ui->polygon_meter = make_polygon_meter();
	ui->polygon_radar = make_polygon_radar();
	initialize_font_cache(ui);

  forge_message_kv(ui, ui->uris.mtr_meters_on, 0, 0);
	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	EBUrUI* ui = (EBUrUI*)handle;
  forge_message_kv(ui, ui->uris.mtr_meters_off, 0, 0);
	if (ui->cpattern) {
		cairo_pattern_destroy (ui->cpattern);
	}
	if (ui->hpattern) {
		cairo_pattern_destroy (ui->hpattern);
	}
	if (ui->fontcache) {
		for (int i=0; i < 6; ++i) {
			pango_font_description_free(ui->font[i]);
		}
	}
	if (ui->polygon_meter) {
		gdk_region_destroy(ui->polygon_meter);
	}
	if (ui->polygon_radar) {
		gdk_region_destroy(ui->polygon_radar);
	}

	free(ui->radarS);
	free(ui->radarM);

	gtkext_rbtn_destroy(ui->cbx_lufs);
	gtkext_rbtn_destroy(ui->cbx_lu);
	gtkext_rbtn_destroy(ui->cbx_sc9);
	gtkext_rbtn_destroy(ui->cbx_sc18);
	gtkext_rbtn_destroy(ui->cbx_sc24);
	gtkext_rbtn_destroy(ui->cbx_ring_short);
	gtkext_rbtn_destroy(ui->cbx_ring_mom);
	gtkext_rbtn_destroy(ui->cbx_hist_short);
	gtkext_rbtn_destroy(ui->cbx_hist_mom);
	gtkext_cbtn_destroy(ui->cbx_transport);
	gtkext_cbtn_destroy(ui->cbx_autoreset);
	gtkext_cbtn_destroy(ui->cbx_truepeak);
	gtkext_spin_destroy(ui->spn_radartime);
	gtkext_cbtn_destroy(ui->btn_start);
	gtkext_pbtn_destroy(ui->btn_reset);
	gtk_widget_destroy(ui->lbl_ringinfo);
	gtk_widget_destroy(ui->lbl_radarinfo);
	gtk_widget_destroy(ui->sep_v0);
	gtk_widget_destroy(ui->m0);
	gtk_widget_destroy(ui->align);

	/* IA__gtk_widget_destroy: assertion `GTK_IS_WIDGET (widget)' fail: */
	//gtk_widget_destroy(ui->cbx_box);

	free(ui);
}

/******************************************************************************
 * handle data from backend
 */

#define PARSE_A_FLOAT(var, dest) \
	if (var && var->type == uris->atom_Float) { \
		dest = ((LV2_Atom_Float*)var)->body; \
	}

#define PARSE_A_INT(var, dest) \
	if (var && var->type == uris->atom_Int) { \
		dest = ((LV2_Atom_Int*)var)->body; \
	}

static void parse_ebulevels(EBUrUI* ui, const LV2_Atom_Object* obj) {
	const EBULV2URIs* uris = &ui->uris;
	LV2_Atom *lm = NULL;
	LV2_Atom *mm = NULL;
	LV2_Atom *ls = NULL;
	LV2_Atom *ms = NULL;
	LV2_Atom *il = NULL;
	LV2_Atom *rn = NULL;
	LV2_Atom *rx = NULL;
	LV2_Atom *ii = NULL;
	LV2_Atom *it = NULL;
	LV2_Atom *tp = NULL;

	lv2_atom_object_get(obj,
			uris->ebu_loudnessM, &lm,
			uris->ebu_maxloudnM, &mm,
			uris->ebu_loudnessS, &ls,
			uris->ebu_maxloudnS, &ms,
			uris->ebu_integrated, &il,
			uris->ebu_range_min, &rn,
			uris->ebu_range_max, &rx,
			uris->mtr_truepeak, &tp,
			uris->ebu_integrating, &ii,
			uris->ebu_integr_time, &it,
			NULL
			);

	PARSE_A_FLOAT(lm, ui->lm)
	PARSE_A_FLOAT(mm, ui->mm)
	PARSE_A_FLOAT(ls, ui->ls)
	PARSE_A_FLOAT(ms, ui->ms)
	PARSE_A_FLOAT(il, ui->il)
	PARSE_A_FLOAT(rn, ui->rn)
	PARSE_A_FLOAT(rx, ui->rx)
	PARSE_A_FLOAT(tp, ui->tp)
	PARSE_A_FLOAT(it, ui->it)

	if (ii && ii->type == uris->atom_Bool) {
		bool ix = ((LV2_Atom_Bool*)ii)->body;
	  bool bx = gtkext_cbtn_get_active(ui->btn_start);
		if (ix != bx) {
			ui->disable_signals = true;
			gtkext_cbtn_set_active(ui->btn_start, ix);
			ui->disable_signals = false;
		}
	}
}

static void parse_radarinfo(EBUrUI* ui, const LV2_Atom_Object* obj) {
	const EBULV2URIs* uris = &ui->uris;
	LV2_Atom *lm = NULL;
	LV2_Atom *ls = NULL;
	LV2_Atom *pp = NULL;
	LV2_Atom *pc = NULL;
	LV2_Atom *pm = NULL;

	float xlm, xls;
	int p,c,m;

	xlm = xls = -INFINITY;
	p=c=m=-1;

	lv2_atom_object_get(obj,
			uris->ebu_loudnessM, &lm,
			uris->ebu_loudnessS, &ls,
			uris->rdr_pointpos, &pp,
			uris->rdr_pos_cur, &pc,
			uris->rdr_pos_max, &pm,
			NULL
			);

	PARSE_A_FLOAT(lm, xlm)
	PARSE_A_FLOAT(ls, xls)
	PARSE_A_INT(pp, p);
	PARSE_A_INT(pc, c);
	PARSE_A_INT(pm, m);

	if (m < 1 || c < 0 || p < 0) return;

	if (m != ui->radar_pos_max) {
		ui->radarS = (float*) realloc((void*) ui->radarS, sizeof(float) * m);
		ui->radarM = (float*) realloc((void*) ui->radarM, sizeof(float) * m);
		ui->radar_pos_max = m;
		for (int i=0; i < ui->radar_pos_max; ++i) {
			ui->radarS[i] = -INFINITY;
			ui->radarM[i] = -INFINITY;
		}
	}
	ui->radarM[p] = xlm;
	ui->radarS[p] = xls;
	ui->radar_pos_cur = c;
}

static void parse_histogram(EBUrUI* ui, const LV2_Atom_Object* obj) {
	const EBULV2URIs* uris = &ui->uris;
	LV2_Atom *lm = NULL;
	LV2_Atom *ls = NULL;
	LV2_Atom *pp = NULL;

	int p = -1;

	lv2_atom_object_get(obj,
			uris->ebu_loudnessM, &lm,
			uris->ebu_loudnessS, &ls,
			uris->rdr_pointpos, &pp,
			NULL
			);

	PARSE_A_INT(pp, p);
	if (p < 0 || p > HIST_LEN) return;
	PARSE_A_INT(lm, ui->histM[p]);
	PARSE_A_INT(ls, ui->histS[p]);

	invalidate_histogram_line(ui, p);
}


static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	EBUrUI* ui = (EBUrUI*)handle;
	const EBULV2URIs* uris = &ui->uris;

	if (format == uris->atom_eventTransfer) {
		LV2_Atom* atom = (LV2_Atom*)buffer;

		if (atom->type == uris->atom_Blank) {
			LV2_Atom_Object* obj = (LV2_Atom_Object*)atom;
			if (obj->body.otype == uris->mtr_ebulevels) {
				parse_ebulevels(ui, obj);
				invalidate_changed(ui, 0);
			} else if (obj->body.otype == uris->mtr_control) {
				int k; float v;
				get_cc_key_value(&ui->uris, obj, &k, &v);
				if (k == CTL_LV2_FTM) {
					int vv = v;
					ui->disable_signals = true;
					gtkext_cbtn_set_active(ui->cbx_autoreset, (vv&2)==2);
					gtkext_cbtn_set_active(ui->cbx_transport, (vv&1)==1);
					ui->disable_signals = false;
				} else if (k == CTL_LV2_RADARTIME) {
					ui->disable_signals = true;
					gtkext_spin_set_value(ui->spn_radartime, v);
					ui->disable_signals = false;
				} else if (k == CTL_LV2_RESETRADAR) {
					for (int i=0; i < ui->radar_pos_max; ++i) {
						ui->radarS[i] = -INFINITY;
						ui->radarM[i] = -INFINITY;
					}
					for (int i=0; i < HIST_LEN; ++i) {
						ui->histM[i] = 0;
						ui->histS[i] = 0;
						ui->histLenM = 0;
						ui->histLenS = 0;
					}
					invalidate_changed(ui, -1);
				} else if (k == CTL_LV2_RESYNCDONE) {
					invalidate_changed(ui, -1);
				} else if (k == CTL_UISETTINGS) {
					uint32_t vv = v;
					ui->disable_signals = true;
					if ((vv & 1)) {
						gtkext_rbtn_set_active(ui->cbx_lufs, true);
					} else {
						gtkext_rbtn_set_active(ui->cbx_lu, true);
					}
					if ((vv & 2)) {
						gtkext_rbtn_set_active(ui->cbx_sc9, true);
					} else {
#ifdef EASTER_EGG
						if ((vv & 32)) {
							gtkext_rbtn_set_active(ui->cbx_sc24, true);
						} else {
							gtkext_rbtn_set_active(ui->cbx_sc18, true);
						}
#else
						gtkext_rbtn_set_active(ui->cbx_sc18, true);
#endif
					}
					if ((vv & 4)) {
						gtkext_rbtn_set_active(ui->cbx_ring_short, true);
					} else {
						gtkext_rbtn_set_active(ui->cbx_ring_mom, true);
					}
					if ((vv & 8)) {
						gtkext_rbtn_set_active(ui->cbx_hist_short, true);
					} else {
						gtkext_rbtn_set_active(ui->cbx_hist_mom, true);
					}
					if ((vv & 16)) {
						gtkext_rbtn_set_active(ui->cbx_histogram, true);
					} else {
						gtkext_rbtn_set_active(ui->cbx_radar, true);
					}
					gtkext_cbtn_set_active(ui->cbx_truepeak, (vv & 64) ? true: false);
					ui->disable_signals = false;
				}
			} else if (obj->body.otype == uris->rdr_radarpoint) {
				parse_radarinfo(ui, obj);
				if (gtkext_rbtn_get_active(ui->cbx_radar)) {
					invalidate_changed(ui, 0);
				}
			} else if (obj->body.otype == uris->rdr_histpoint) {
				parse_histogram(ui, obj);
			} else if (obj->body.otype == uris->rdr_histogram) {
				LV2_Atom *lm = NULL;
				LV2_Atom *ls = NULL;
				lv2_atom_object_get(obj,
						uris->ebu_loudnessM, &lm,
						uris->ebu_loudnessS, &ls,
				NULL
				);
				PARSE_A_INT(lm, ui->histLenM);
				PARSE_A_INT(ls, ui->histLenS);
				if (gtkext_rbtn_get_active(ui->cbx_histogram)) {
					invalidate_changed(ui, 3);
				}
			} else {
				fprintf(stderr, "UI: Unknown control message.\n");
			}
		} else {
			fprintf(stderr, "UI: Unknown message type.\n");
		}
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
	"http://gareus.org/oss/lv2/meters#eburui",
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
