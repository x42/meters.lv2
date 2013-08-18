/* goniometer LV2 GUI
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


typedef void Stcorrdsp;

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#include "zita-resampler/resampler.h"
#include "goniometer.h"
#include "gtkextdial.h"

#define GED_W(PTR) gtkext_dial_wiget(PTR)

#define PC_BOUNDS ( 40.0f)

#define PC_TOP    ( 12.5f)
#define PC_BLOCK  (  8.0f)
#define PC_LEFT   ( 10.0f)
#define PC_WIDTH  ( 20.0f)
#define PC_HEIGHT (380.0f)

#define GM_BOUNDS (405.0f)
#define GM_CENTER (202.5f)

#define GM_RADIUS (200.0f)
#define GM_RAD2   (100.0f)

#define MAX_CAIRO_PATH 32

using namespace LV2M;

typedef struct {
	LV2_Handle instance;
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	GtkWidget* box;
	GtkWidget* align;
	GtkWidget* m0;

	GtkWidget* c_tbl;

	GtkWidget* cbx_src;
	GtkWidget* spn_src_fact;

	GtkExtDial* spn_compress;
	GtkExtDial* spn_gattack;
	GtkExtDial* spn_gdecay;
	GtkExtDial* spn_gtarget;
	GtkExtDial* spn_grms;

	GtkWidget* cbx_lines;
	GtkWidget* cbx_xfade;

	GtkWidget* spn_psize;
	GtkWidget* spn_vfreq;
	GtkExtDial* spn_alpha;

	GtkWidget* sep_h0;
	GtkWidget* sep_v0;

	GtkWidget* lbl_src_fact;
	GtkWidget* lbl_psize;
	GtkWidget* lbl_vfreq;
	GtkWidget* lbl_compress;
	GtkWidget* lbl_gattack;
	GtkWidget* lbl_gdecay;
	GtkWidget* lbl_gtarget;
	GtkWidget* lbl_grms;

	GtkWidget* fader;
	GtkWidget* cbx_autogain;

	int sfc;
	cairo_surface_t* sf[3];
	cairo_surface_t* an[7];
	cairo_surface_t* dial[4];

	float last_x, last_y;
	float lp0, lp1;
	float hpw;

	float cor, cor_u;
	uint32_t ntfy_u, ntfy_b;

	float gain;
	bool disable_signals;

	float attack_pow;
	float decay_pow;
	float g_target;
	float g_rms;

	Resampler *src;
	float *scratch;
	float *resampl;
	float src_fact;
} GMUI;

static void setup_src(GMUI* ui, float oversample, int hlen, float frel) {
	if (ui->src != 0) {
		delete ui->src;
		free(ui->scratch);
		free(ui->resampl);
		ui->src = 0;
		ui->scratch = 0;
		ui->resampl = 0;
	}

	if (oversample <= 1) {
		ui->src_fact = 1;
		return;
	}

	LV2gm* self = (LV2gm*) ui->instance;
	uint32_t bsiz = self->rate * 2;

	ui->src_fact = oversample;
	ui->src = new Resampler();
	ui->src->setup(self->rate, self->rate * oversample, 2, hlen, frel);

	ui->scratch = (float*) calloc(bsiz, sizeof(float));
	ui->resampl = (float*) malloc(bsiz * oversample * sizeof(float));

	/* q/d initialize */
	ui->src->inp_count = 8192;
	ui->src->inp_data = ui->scratch;
	ui->src->out_count = 8192 * oversample;
	ui->src->out_data = ui->resampl;
	ui->src->process ();
}

/*****
 * drawing helpers
 */

static void write_text(
		cairo_t* cr,
		const char *txt, const char * font,
		const int align,
		const float x, const float y) {
	int tw, th;
	cairo_save(cr);

	PangoLayout * pl = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string(font);
	pango_layout_set_font_description(pl, fd);
	pango_font_description_free(fd);
	if (align < 0) {
		cairo_set_source_rgba (cr, .9, .95, .9, 1.0);
	} else {
		cairo_set_source_rgba (cr, .5, .5, .6, 1.0);
	}
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, &tw, &th);
	cairo_translate (cr, x, y);
	switch(abs(align)) {
		default:
			cairo_translate (cr, -tw/2.0 - 0.5, -th/2.0);
			break;
		case 3:
			cairo_translate (cr, -.5 , -th);
			break;
		case 4:
			cairo_translate (cr, -tw - 0.5, -th);
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

static void alloc_annotations(GMUI* ui) {
#define FONT_GM "Mono 16"
#define FONT_PC "Mono 10"
#define FONT_LB "Sans 06"

#define INIT_BLACK_BG(ID, WIDTH, HEIGHT) \
	ui->an[ID] = cairo_image_surface_create (CAIRO_FORMAT_RGB24, WIDTH, HEIGHT); \
	cr = cairo_create (ui->an[ID]); \
	cairo_set_source_rgb (cr, .0, .0, .0); \
	cairo_rectangle (cr, 0, 0, WIDTH, WIDTH); \
	cairo_fill (cr);

	cairo_t* cr;

	INIT_BLACK_BG(0, 32, 32)
	write_text(cr, "L", FONT_GM, 0, 16, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(1, 32, 32)
	write_text(cr, "R", FONT_GM, 0, 16, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(2, 64, 32)
	write_text(cr, "Mono", FONT_GM, 0, 32, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(3, 32, 32)
	write_text(cr, "+S", FONT_GM, 0, 16, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(4, 32, 32)
	write_text(cr, "-S", FONT_GM, 0, 16, 16);
	cairo_destroy (cr);

	INIT_BLACK_BG(5, 32, 32)
	write_text(cr, "+1", FONT_PC, 0, 10, 10);
	cairo_destroy (cr);

	INIT_BLACK_BG(6, 32, 32)
	write_text(cr, "-1", FONT_PC, 0, 10, 10);
	cairo_destroy (cr);

#define INIT_DIAL_SF(VAR, TXTL, TXTR) \
	VAR = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, GED_WIDTH, GED_HEIGHT); \
	cr = cairo_create (VAR); \
	cairo_set_source_rgba (cr, .0, .0, .0, 0); \
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE); \
	cairo_rectangle (cr, 0, 0, GED_WIDTH, GED_HEIGHT); \
	cairo_fill (cr); \
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER); \
	write_text(cr, TXTL, FONT_LB, -3, 1, GED_HEIGHT - 1); \
	write_text(cr, TXTR, FONT_LB, -4, GED_WIDTH-1, GED_HEIGHT - 1); \
	cairo_destroy (cr);

	INIT_DIAL_SF(ui->dial[0], "fast", "slow")
	INIT_DIAL_SF(ui->dial[1], "peak", "rms ")
	INIT_DIAL_SF(ui->dial[2], "  0%", "100%")
	INIT_DIAL_SF(ui->dial[3], " 15%", "600%")
}

static void alloc_sf(GMUI* ui) {
	cairo_t* cr;
#define ALLOC_SF(VAR) \
	VAR = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, GM_BOUNDS, GM_BOUNDS);\
	cr = cairo_create (VAR);\
	cairo_set_source_rgb (cr, .0, .0, .0);\
	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);\
	cairo_rectangle (cr, 0, 0, GM_BOUNDS, GM_BOUNDS);\
	cairo_fill (cr);\
	cairo_destroy(cr);

	ALLOC_SF(ui->sf[0])
	ALLOC_SF(ui->sf[1])
	ALLOC_SF(ui->sf[2])
}


static void draw_rb(GMUI* ui, gmringbuf *rb) {
	float d0, d1;
	size_t n_samples = gmrb_read_space(rb);
	if (n_samples < 64) return;

	const bool composit = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_xfade));
	const bool autogain = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_autogain));
	const bool lines = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_lines));
	const float line_width = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_psize));
	const float compress = .02 * gtkext_dial_get_value(ui->spn_compress);
	const float persist = .5 + .005 * gtkext_dial_get_value(ui->spn_alpha);
	const float attack_pow = ui->attack_pow;
	const float decay_pow = ui->decay_pow;
	const float g_target = ui->g_target;
	const float g_rms = ui->g_rms;

	if (composit) {
		ui->sfc = (ui->sfc + 1) % 3;
	}
	cairo_t* cr = cairo_create (ui->sf[ui->sfc]);

	if (composit) {
		cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_rgba (cr, .0, .0, .0, /* 1.0-persist*/ .42);
		cairo_rectangle (cr, 0, 0, GM_BOUNDS, GM_BOUNDS);
		cairo_fill (cr);
	} else if (persist >= 1.0) {
		;
	} else if (persist >  0.5) {
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgba (cr, .0, .0, .0, 1.0-persist);
		cairo_rectangle (cr, 0, 0, GM_BOUNDS, GM_BOUNDS);
		cairo_fill (cr);
	} else {
		cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
		cairo_rectangle (cr, 0, 0, GM_BOUNDS, GM_BOUNDS);
		cairo_fill (cr);
	}

	cairo_rectangle (cr, 0, 0, GM_BOUNDS, GM_BOUNDS);
	cairo_clip(cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba (cr, .88, .88, .15, .8); // GM COLOR

	int cnt = 0;
	if (lines) {
		//cairo_set_tolerance(cr, 1.0); // default .1
		cairo_set_line_width(cr, line_width);
		cairo_move_to(cr, ui->last_x, ui->last_y);
	} else {
		cairo_set_line_width(cr, line_width);
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	}

	bool os = false;
	size_t n_points = n_samples;
	if (ui->src_fact > 1) {
		size_t j=0;
		for (j=0; j < n_samples; ++j) {
			if (gmrb_read_one(rb, &ui->scratch[2*j], &ui->scratch[2*j+1])) break;
		}

		assert (j == n_samples);
		assert (n_samples > 0);

		ui->src->inp_count = n_samples;
		ui->src->inp_data = ui->scratch;
		ui->src->out_count = n_samples * ui->src_fact;
		ui->src->out_data = ui->resampl;
		ui->src->process ();
		n_points *= ui->src_fact;
		os = true;
	}

	float rms_0 = 0;
	float rms_1 = 0;
	int   rms_c = 0;
	float ag_xmax = 0;
	float ag_ymax = 0;
	float ag_xmin = 0;
	float ag_ymin = 0;

	for (uint32_t i=0; i < n_points; ++i) {
		if (os) {
			d0 = ui->resampl[2*i];
			d1 = ui->resampl[2*i+1];
		} else {
			if (gmrb_read_one(rb, &d0, &d1)) break;
		}

#if 1 /* high pass filter */
		ui->lp0 += ui->hpw * (d0 - ui->lp0);
		ui->lp1 += ui->hpw * (d1 - ui->lp1);
		/* prevent denormals */
		ui->lp0 += 1e-12f;
		ui->lp1 += 1e-12f;
#else
		ui->lp0 = d0;
		ui->lp1 = d1;
#endif
		const float ax = (ui->lp0 - ui->lp1);
		const float ay = (ui->lp0 + ui->lp1);

		if (autogain) {
			if (ax > ag_xmax) ag_xmax = ax;
			if (ax < ag_xmin) ag_xmin = ax;
			if (ay > ag_ymax) ag_ymax = ay;
			if (ay < ag_ymin) ag_ymin = ay;
			rms_0 += ui->lp0 * ui->lp0;
			rms_1 += ui->lp1 * ui->lp1;
			rms_c++;
		}

		float x, y;
		if (compress > 0.0 && compress <= 2.0) {
			const float volume = sqrt(ax * ax + ay * ay);
			float compr = 1.0;
			if (volume > 1.0) {
				compr = 1.0 / volume;
			} else if (volume > .001) {
				compr = 1.0 - compress * log10(volume);
			} else {
				compr = 1.0 + compress * 3.0;
			}
			x = GM_CENTER - ui->gain * ax * compr * GM_RAD2;
			y = GM_CENTER - ui->gain * ay * compr * GM_RAD2;
		} else {
			x = GM_CENTER - ui->gain * ax * GM_RAD2;
			y = GM_CENTER - ui->gain * ay * GM_RAD2;
		}

		if ( (ui->last_x - x) * (ui->last_x - x) + (ui->last_y - y) * (ui->last_y - y) < 2.0) continue;

		ui->last_x = x;
		ui->last_y = y;

		if (lines) {
			cairo_line_to(cr, ui->last_x, ui->last_y);
		} else {
			cairo_move_to(cr, ui->last_x, ui->last_y);
			cairo_close_path (cr);
		}

		if (++cnt > MAX_CAIRO_PATH) {
			cnt = 0;
			cairo_stroke(cr);
			if (lines) {
				cairo_move_to(cr, ui->last_x, ui->last_y);
			}
		}
	}

	if (cnt > 0) {
		cairo_stroke(cr);
	}

	cairo_destroy(cr);

	if (autogain) {
		LV2gm* self = (LV2gm*) ui->instance;
		float elapsed = n_samples / self->rate;
		const float xdif = (ag_xmax - ag_xmin);
		const float ydif = (ag_ymax - ag_ymin);
		float max  = sqrt(xdif * xdif + ydif * ydif);

		max *= .707;

		if (rms_c > 0 && g_rms > 0) {
			const float rms = 5.436 /* 2e */ * (rms_0 > rms_1 ? sqrt(rms_0 / rms_c) : sqrt(rms_1 / rms_c));
			//printf("max: %f <> rms %f (tgt:%f)\n", max, rms, g_target);
			max = max * (1.0 - g_rms) + rms * g_rms;
		}

		max *= g_target;

		float gain;
		if (max < .01) {
			gain = 100.0;
		} else if (max > 100.0) {
			gain = .02;
		} else {
			gain = 2.0 / max;
		}

		float attack = gain < ui->gain ? attack_pow * (.31 + .1 * log10f(elapsed)) : decay_pow * (.03 + .007 * logf(elapsed));
		//printf(" %.3f  %.3f [max: %f %f] %f\n", ui->gain, gain, xdif, ydif, max);
		gain = ui->gain + attack * (gain - ui->gain);
		if (gain < .001) gain = .001;

		float fgain = gain;
		if (fgain > 6.0) fgain = 6.0;
		if (fgain < .001) fgain = .001;
		if ( rint(50 * fgain) != rint(50 * gtk_range_get_value(GTK_RANGE(ui->fader)))) {
			gtk_range_set_value(GTK_RANGE(ui->fader), fgain);
		}
		//printf("autogain:  %+6.2f dB (*%f)\n", 20 * log10f(ui->gain), ui->gain);
		ui->gain = gain;
	}
}

static void draw_gm_labels(GMUI* ui, cairo_t* cr) {
	cairo_save(cr);
	cairo_translate(cr, PC_BOUNDS, 0);
	cairo_set_operator (cr, CAIRO_OPERATOR_SCREEN);

#define DRAW_LABEL(ID, XPOS, YPOS) \
	cairo_set_source_surface(cr, ui->an[ID], (XPOS)-16, (YPOS)-16); cairo_paint (cr);

	DRAW_LABEL(0, GM_CENTER - GM_RAD2, GM_CENTER - GM_RAD2)
	DRAW_LABEL(1, GM_CENTER + GM_RAD2, GM_CENTER - GM_RAD2);

	DRAW_LABEL(2, GM_CENTER - 16, GM_CENTER - GM_RADIUS * 3/4 - 12);
	DRAW_LABEL(3, GM_CENTER - GM_RADIUS * 3/4 - 12 , GM_CENTER - 1);
	DRAW_LABEL(4, GM_CENTER + GM_RADIUS * 3/4 + 12 , GM_CENTER - 1);

	const double dashed[] = {1.0, 2.0};
	cairo_set_line_width(cr, 3.5);
	cairo_set_source_rgba (cr, .5, .5, .6, 1.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

	cairo_set_dash(cr, dashed, 2, 0);
	cairo_move_to(cr, GM_CENTER - GM_RADIUS * 0.7079, GM_CENTER);
	cairo_line_to(cr, GM_CENTER + GM_RADIUS * 0.7079, GM_CENTER);
	cairo_stroke(cr);

	cairo_move_to(cr, GM_CENTER, GM_CENTER - GM_RADIUS * 0.7079);
	cairo_line_to(cr, GM_CENTER, GM_CENTER + GM_RADIUS * 0.7079);
	cairo_stroke(cr);

	cairo_restore(cr);
}

static void draw_pc_annotation(GMUI* ui, cairo_t* cr) {
	cairo_set_operator (cr, CAIRO_OPERATOR_SCREEN);
	cairo_set_source_rgba (cr, .5, .5, .6, 1.0);
	cairo_set_line_width(cr, 1.5);

#define PC_ANNOTATION(YPOS) \
	cairo_move_to(cr, PC_LEFT + 2.0, PC_TOP + YPOS); \
	cairo_line_to(cr, PC_LEFT + PC_WIDTH - 2.0, PC_TOP + YPOS);\
	cairo_stroke(cr);

	PC_ANNOTATION(PC_HEIGHT * 0.1);
	PC_ANNOTATION(PC_HEIGHT * 0.2);
	PC_ANNOTATION(PC_HEIGHT * 0.3);
	PC_ANNOTATION(PC_HEIGHT * 0.4);
	PC_ANNOTATION(PC_HEIGHT * 0.6);
	PC_ANNOTATION(PC_HEIGHT * 0.7);
	PC_ANNOTATION(PC_HEIGHT * 0.8);
	PC_ANNOTATION(PC_HEIGHT * 0.9);

	DRAW_LABEL(5, PC_LEFT + 16, PC_TOP + 14);
	DRAW_LABEL(6, PC_LEFT + 16, PC_TOP + PC_HEIGHT - 2);

	cairo_set_line_width(cr, 2.0);
	cairo_set_source_rgba (cr, .7, .7, .8, 1.0);
	PC_ANNOTATION(PC_HEIGHT * 0.5);
}

static gboolean expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	LV2gm* self = (LV2gm*) ui->instance;

	/* process and draw goniometer data */
	if (ui->ntfy_b != ui->ntfy_u) {
		//printf("%d -> %d\n", ui->ntfy_u, ui->ntfy_b);
		ui->ntfy_u = ui->ntfy_b;
		draw_rb(ui, self->rb);
	}

	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	/* display goniometer */
	const bool composit = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_xfade));
	if (!composit) {
		cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_surface(cr, ui->sf[ui->sfc], PC_BOUNDS, 0);
		cairo_paint (cr);
	} else {
		// TODO tweak and optimize overlay compositing

		cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_surface(cr, ui->sf[(ui->sfc + 1)%3], PC_BOUNDS, 0);
		cairo_paint (cr);

		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

		cairo_set_source_surface(cr, ui->sf[(ui->sfc + 0)%3], PC_BOUNDS, 0);
		cairo_paint (cr);

		cairo_set_source_surface(cr, ui->sf[(ui->sfc + 2)%3], PC_BOUNDS, 0);
		cairo_paint (cr);
	}

	draw_gm_labels(ui, cr);

	/* display phase-correlation */
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	/* PC meter backgroud */
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	cairo_rectangle (cr, 0, 0, PC_BOUNDS, GM_BOUNDS);
	cairo_fill(cr);

	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_set_line_width(cr, 1.0);
	rounded_rectangle (cr, PC_LEFT -1.0, PC_TOP - 2.0 , PC_WIDTH + 2.0, PC_HEIGHT + 4.0, 6);
	cairo_fill(cr);

	/* value */
	cairo_set_source_rgba (cr, .7, .7, .1, 1.0); // PC COLOR
	const float c = (PC_HEIGHT - PC_BLOCK) * ui->cor;
	rounded_rectangle (cr, PC_LEFT, PC_TOP + c, PC_WIDTH, PC_BLOCK, 2);
	cairo_fill(cr);

	draw_pc_annotation(ui, cr);

	cairo_destroy (cr);

	return TRUE;
}

/******************************************************************************
 * UI callbacks
 */

static void save_state(GMUI* ui) {
	LV2gm* self = (LV2gm*) ui->instance;
	self->s_autogain   = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_autogain));
	self->s_oversample = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_src));
	self->s_line       = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_lines));
	self->s_persist    = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_xfade));

	self->s_sfact = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_src_fact));
	if (self->s_line) {
		self->s_linewidth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_psize));
	} else {
		self->s_pointwidth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_psize));
	}
	self->s_persistency = gtkext_dial_get_value(ui->spn_alpha);
	self->s_max_freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_vfreq));

	self->s_gattack = gtkext_dial_get_value(ui->spn_gattack);
	self->s_gdecay = gtkext_dial_get_value(ui->spn_gdecay);
	self->s_compress = gtkext_dial_get_value(ui->spn_compress);

	self->s_gtarget = gtkext_dial_get_value(ui->spn_gtarget);
	self->s_grms = gtkext_dial_get_value(ui->spn_grms);
}

static gboolean cb_save_state(GtkWidget *w, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	save_state(ui);
	return TRUE;
}

static gboolean set_gain(GtkRange* r, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	ui->gain = gtk_range_get_value(r);
	const bool autogain = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_autogain));
	if (!ui->disable_signals && !autogain) {
		ui->write(ui->controller, 4, sizeof(float), 0, (const void*) &ui->gain);
	}
	return TRUE;
}

static gboolean cb_autogain(GtkWidget *w, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	const bool autogain = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_autogain));
	if (autogain) {
		gtk_widget_set_sensitive(GTK_WIDGET(ui->fader), false);
		gtkext_dial_set_sensitive(ui->spn_gattack, true);
		gtkext_dial_set_sensitive(ui->spn_gdecay, true);
		gtkext_dial_set_sensitive(ui->spn_gtarget, true);
		gtkext_dial_set_sensitive(ui->spn_grms, true);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(ui->fader), true);
		gtkext_dial_set_sensitive(ui->spn_gattack, false);
		gtkext_dial_set_sensitive(ui->spn_gdecay, false);
		gtkext_dial_set_sensitive(ui->spn_gtarget, false);
		gtkext_dial_set_sensitive(ui->spn_grms, false);
		ui->write(ui->controller, 4, sizeof(float), 0, (const void*) &ui->gain);
	}
	save_state(ui);
	return TRUE;
}

static gboolean cb_autosettings(GtkWidget *w, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	float g_attack = gtkext_dial_get_value(ui->spn_gattack);
	float g_decay = gtkext_dial_get_value(ui->spn_gdecay);
	g_attack = 0.1 * exp(0.06 * g_attack) - .09;
	g_decay  = 0.1 * exp(0.06 * g_decay ) - .09;
	if (g_attack < .01) g_attack = .01;
	if (g_decay  < .01) g_decay = .01;
	ui->attack_pow = g_attack;
	ui->decay_pow = g_decay;
	//
	float g_rms = .01 * gtkext_dial_get_value(ui->spn_grms);
	ui->g_rms = g_rms;

	float g_target = gtkext_dial_get_value(ui->spn_gtarget);
	g_target = exp(1.8 * (-.02 * g_target + 1.0));
	if (g_target < .15) g_target = .15;
	ui->g_target = g_target;
	save_state(ui);
	return TRUE;
}

static gboolean cb_expose(GtkWidget *w, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	gtk_widget_queue_draw(ui->m0);
	save_state(ui);
	return TRUE;
}

static gboolean cb_lines(GtkWidget *w, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	LV2gm* self = (LV2gm*) ui->instance;
	const bool nowlines = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_lines));
	if (!nowlines) {
		self->s_linewidth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_psize));
	} else {
		self->s_pointwidth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_psize));
	}
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_psize), nowlines ? self->s_linewidth : self->s_pointwidth);
	return cb_expose(w, handle);
}

static gboolean cb_xfade(GtkWidget *w, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_xfade))) {
		gtkext_dial_set_sensitive(ui->spn_alpha, false);
	} else {
		gtkext_dial_set_sensitive(ui->spn_alpha, true);
	}
	return cb_expose(w, handle);
}

static gboolean cb_vfreq(GtkWidget *w, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	LV2gm* self = (LV2gm*) ui->instance;
	float v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_vfreq));
	if (v < 10) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_vfreq), 10);
		return TRUE;
	}
	if (v > 100) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_vfreq), 100);
		return TRUE;
	}

	v = rint(self->rate / v);

	self->apv = v;
	save_state(ui);
	return TRUE;
}

static gboolean cb_src(GtkWidget *w, gpointer handle) {
	GMUI* ui = (GMUI*)handle;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cbx_src))) {
		setup_src(ui, gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->spn_src_fact)), 8, .7);
	} else {
		setup_src(ui, 0, 0, 0);
	}
	save_state(ui);
	return TRUE;
}

static void restore_state(GMUI* ui) {
	LV2gm* self = (LV2gm*) ui->instance;
	ui->disable_signals = true;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_src_fact), self->s_sfact);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_psize), self->s_line ? self->s_linewidth : self->s_pointwidth);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_vfreq), self->s_max_freq);
	gtkext_dial_set_value(ui->spn_alpha, self->s_persistency);

	gtkext_dial_set_value(ui->spn_gattack, self->s_gattack);
	gtkext_dial_set_value(ui->spn_gdecay, self->s_gdecay);
	gtkext_dial_set_value(ui->spn_compress, self->s_compress);
	gtkext_dial_set_value(ui->spn_gtarget, self->s_gtarget);
	gtkext_dial_set_value(ui->spn_grms, self->s_grms);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cbx_autogain), self->s_autogain  );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cbx_src),      self->s_oversample);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cbx_lines),    self->s_line      );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cbx_xfade),    self->s_persist   );
	// TODO optimize, temp disable save during these
	cb_autogain(NULL, ui);
	cb_src(NULL, ui);
	cb_vfreq(NULL, ui);
	cb_xfade(NULL, ui);
	cb_autosettings(NULL, ui);
	ui->disable_signals = false;
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
	GMUI* ui = (GMUI*) calloc(1,sizeof(GMUI));
	*widget = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, "http://lv2plug.in/ns/ext/instance-access")) {
			ui->instance = (LV2_Handle*)features[i]->data;
		}
	}

	if (!ui->instance) {
		fprintf(stderr, "meters.lv2 error: Host does not support instance-access\n");
		free(ui);
		return NULL;
	}

	LV2gm* self = (LV2gm*) ui->instance;

	ui->write      = write_function;
	ui->controller = controller;

	alloc_sf(ui);
	alloc_annotations(ui);

	ui->last_x = (GM_CENTER);
	ui->last_y = (GM_CENTER);
	ui->lp0 = 0;
	ui->lp1 = 0;
	ui->hpw = expf(-2.0 * M_PI * 20 / self->rate);
	ui->gain = 1.0;

	ui->cor = 0.5;
	ui->cor_u = 0.5;
	ui->ntfy_b = 0;
	ui->ntfy_u = 1;
	ui->disable_signals = false;
	ui->src_fact = 1;

	ui->box   = gtk_vbox_new(FALSE, 2);
	ui->align = gtk_alignment_new(.5, .5, 0, 0);
	ui->m0    = gtk_drawing_area_new();
	ui->fader = gtk_hscale_new_with_range(0, 6.0, .001);
	ui->cbx_autogain = gtk_check_button_new_with_label("Auto Gain");

	ui->c_tbl        = gtk_table_new(/*rows*/6, /*cols*/ 6, FALSE);
	ui->cbx_src      = gtk_check_button_new_with_label("Oversample");
	ui->spn_src_fact = gtk_spin_button_new_with_range(1, 32, 1);

	ui->spn_compress = gtkext_dial_new(0.0, 100.0, 0.5);
	ui->spn_gattack  = gtkext_dial_new(0.0, 100.0, 1.0);
	ui->spn_gdecay   = gtkext_dial_new(0.0, 100.0, 1.0);

	ui->spn_gtarget  = gtkext_dial_new(0.0, 100.0, 1.0);
	ui->spn_grms     = gtkext_dial_new(0.0, 100.0, 1.0);

	ui->cbx_lines    = gtk_check_button_new_with_label("Draw Lines");
	ui->cbx_xfade    = gtk_check_button_new_with_label("CRT Persistency:");
	ui->spn_psize    = gtk_spin_button_new_with_range(.25, 5.25, .25);

	ui->spn_vfreq    = gtk_spin_button_new_with_range(10, 100, 5);
	ui->spn_alpha    = gtkext_dial_new(0, 100, .5);

	gtkext_dial_set_value(ui->spn_compress, 0.0);
	gtkext_dial_set_value(ui->spn_gattack, 1.0);
	gtkext_dial_set_value(ui->spn_gdecay, 1.0);

	gtkext_dial_set_value(ui->spn_gtarget, 50.0);
	gtkext_dial_set_value(ui->spn_grms, 0.0);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_src_fact), 4.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_psize), 1.25);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spn_vfreq), 25);
	gtkext_dial_set_value(ui->spn_alpha, 0);

	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(ui->spn_psize), 2);

	ui->sep_h0        = gtk_hseparator_new();
	ui->sep_v0        = gtk_vseparator_new();

	ui->lbl_src_fact  = gtk_label_new("Factor:");
	ui->lbl_psize     = gtk_label_new("Pixels:");
	ui->lbl_vfreq     = gtk_label_new("Max. Update Freq [Hz]:");
	ui->lbl_compress  = gtk_label_new("Inflate:");
	ui->lbl_gattack   = gtk_label_new("Attack Speed:");
	ui->lbl_gdecay    = gtk_label_new("Decay Speed:");
	ui->lbl_gtarget   = gtk_label_new("Target Zoom:");
	ui->lbl_grms      = gtk_label_new("RMS (vs peak):");

	gtk_misc_set_alignment(GTK_MISC(ui->lbl_vfreq),    1.0f, 0.5f);
	gtk_misc_set_alignment(GTK_MISC(ui->lbl_compress), 1.0f, 0.5f);
	gtk_misc_set_alignment(GTK_MISC(ui->lbl_gattack),  1.0f, 0.5f);
	gtk_misc_set_alignment(GTK_MISC(ui->lbl_gdecay),   1.0f, 0.5f);
	gtk_misc_set_alignment(GTK_MISC(ui->lbl_gtarget),  1.0f, 0.5f);
	gtk_misc_set_alignment(GTK_MISC(ui->lbl_grms),     1.0f, 0.5f);
	gtk_misc_set_alignment(GTK_MISC(ui->lbl_src_fact), 1.0f, 0.5f);
	gtk_misc_set_alignment(GTK_MISC(ui->lbl_psize),    1.0f, 0.5f);

	gtkext_dial_set_surface(ui->spn_gattack,  ui->dial[0]);
	gtkext_dial_set_surface(ui->spn_gdecay,   ui->dial[0]);
	gtkext_dial_set_surface(ui->spn_gtarget,  ui->dial[3]);
	gtkext_dial_set_surface(ui->spn_grms,     ui->dial[1]);
	gtkext_dial_set_surface(ui->spn_compress, ui->dial[2]);
	gtkext_dial_set_surface(ui->spn_alpha,    ui->dial[2]);

	/* fader init */
	gtk_scale_set_draw_value(GTK_SCALE(ui->fader), FALSE);
	gtk_range_set_value(GTK_RANGE(ui->fader), 1.0);
#if 1
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 5.6234, GTK_POS_TOP, "" /* "+15dB"*/);
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 3.9810, GTK_POS_TOP, "+12dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 2.8183, GTK_POS_TOP, "" /* "+9dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 1.9952, GTK_POS_TOP, "+6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 1.4125, GTK_POS_TOP, "" /* "+3dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 1.0000, GTK_POS_TOP, "0dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.7079, GTK_POS_TOP, "" /* "-3dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.5012, GTK_POS_TOP, "-6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.3548, GTK_POS_TOP, "" /* "-9dB" */);
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.2511, GTK_POS_TOP, "-12dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.1778, GTK_POS_TOP, "" /* "-15dB"*/);
#else
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 5.6234, GTK_POS_BOTTOM, "+15dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 3.9810, GTK_POS_TOP, "+12dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 2.8183, GTK_POS_BOTTOM, "+9dB" );
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 1.9952, GTK_POS_TOP, "+6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 1.4125, GTK_POS_BOTTOM, "+3dB" );
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.7079, GTK_POS_BOTTOM, "-3dB" );
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.5012, GTK_POS_TOP, "-6dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.3548, GTK_POS_BOTTOM, "-9dB" );
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.1778, GTK_POS_BOTTOM, "-15dB");
	gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.2511, GTK_POS_TOP, "-12dB");
	//gtk_scale_add_mark(GTK_SCALE(ui->fader), 0.0,  GTK_POS_BOTTOM, ""); // -inf
#endif

	gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), PC_BOUNDS + GM_BOUNDS, GM_BOUNDS);
	gtk_widget_set_size_request(ui->m0, PC_BOUNDS + GM_BOUNDS, GM_BOUNDS);
	gtk_widget_set_redraw_on_allocate(ui->m0, TRUE);

	/* layout */
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), ui->fader              , 0, 6, 0, 1);
	int row = 1;
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), ui->cbx_autogain       , 0, 1, row, row+1);
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->lbl_gattack                 , 1, 2, row, row+1, GTK_FILL, GTK_FILL, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), GED_W(ui->spn_gattack) , 2, 3, row, row+1);
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->lbl_gdecay                  , 4, 5, row, row+1, GTK_FILL, GTK_FILL, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), GED_W(ui->spn_gdecay)  , 5, 6, row, row+1);

	row++;
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->lbl_gtarget                 , 1, 2, row, row+1, GTK_FILL, GTK_FILL, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), GED_W(ui->spn_gtarget) , 2, 3, row, row+1);
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->lbl_grms                    , 4, 5, row, row+1, GTK_FILL, GTK_FILL, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), GED_W(ui->spn_grms)    , 5, 6, row, row+1);

	row++;
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->sep_h0, 0, 6, row, row+1, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), GTK_SHRINK, 0, 4);

	row++;
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->sep_v0, 3, 4, row, row+3, GTK_SHRINK, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 6, 0);

	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), ui->cbx_src            , 0, 1, row, row+1);
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->lbl_src_fact                , 1, 2, row, row+1, GTK_FILL, GTK_FILL, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), ui->spn_src_fact       , 2, 3, row, row+1);

	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->cbx_xfade                   , 4, 5, row, row+1, GTK_SHRINK, GTK_SHRINK, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), GED_W(ui->spn_alpha)   , 5, 6, row, row+1);

	row++;
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), ui->cbx_lines          , 0, 1, row, row+1);
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->lbl_psize                   , 1, 2, row, row+1, GTK_FILL, GTK_FILL, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), ui->spn_psize          , 2, 3, row, row+1);

	row++;
	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->lbl_vfreq                   , 0, 2, row, row+1, GTK_FILL, GTK_FILL, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), ui->spn_vfreq          , 2, 3, row, row+1);

	gtk_table_attach(GTK_TABLE(ui->c_tbl), ui->lbl_compress                , 4, 5, row, row+1, GTK_FILL, GTK_FILL, 4, 0);
	gtk_table_attach_defaults(GTK_TABLE(ui->c_tbl), GED_W(ui->spn_compress), 5, 6, row, row+1);

	gtk_container_add(GTK_CONTAINER(ui->align), ui->m0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->align, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->c_tbl, FALSE, FALSE, 0);

	restore_state(ui);

	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);
	g_signal_connect (G_OBJECT (ui->fader), "value-changed", G_CALLBACK (set_gain), ui);
	g_signal_connect (G_OBJECT (ui->cbx_autogain), "toggled", G_CALLBACK (cb_autogain), ui);

	g_signal_connect (G_OBJECT (ui->cbx_src), "toggled", G_CALLBACK (cb_src), ui);
	g_signal_connect (G_OBJECT (ui->spn_src_fact), "value-changed", G_CALLBACK (cb_src), ui);

	gtkext_dial_set_callback(ui->spn_gattack, cb_autosettings, ui);
	gtkext_dial_set_callback(ui->spn_gdecay, cb_autosettings, ui);
	gtkext_dial_set_callback(ui->spn_gtarget, cb_autosettings, ui);
	gtkext_dial_set_callback(ui->spn_grms, cb_autosettings, ui);

	gtkext_dial_set_callback(ui->spn_compress, cb_expose, ui);
	gtkext_dial_set_callback(ui->spn_alpha, cb_save_state, ui);

	g_signal_connect (G_OBJECT (ui->cbx_lines), "toggled", G_CALLBACK (cb_lines), ui);
	g_signal_connect (G_OBJECT (ui->cbx_xfade), "toggled", G_CALLBACK (cb_xfade), ui);
	g_signal_connect (G_OBJECT (ui->spn_psize), "value-changed", G_CALLBACK (cb_expose), ui);
	g_signal_connect (G_OBJECT (ui->spn_vfreq), "value-changed", G_CALLBACK (cb_vfreq), ui);

	gtk_widget_show_all(ui->box);
	*widget = ui->box;

	gmrb_read_clear(self->rb);
	self->ui_active = true;
	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	GMUI* ui = (GMUI*)handle;
	LV2gm* i = (LV2gm*)ui->instance;

	i->ui_active = false;

	for (int i=0; i < 3 ; ++i) {
		cairo_surface_destroy(ui->sf[i]);
	}

	for (int i=0; i < 7 ; ++i) {
		cairo_surface_destroy(ui->an[i]);
	}
	for (int i=0; i < 4 ; ++i) {
		cairo_surface_destroy(ui->dial[i]);
	}

	gtk_widget_destroy(ui->m0);
	gtk_widget_destroy(ui->fader);
	gtk_widget_destroy(ui->cbx_autogain);
	gtk_widget_destroy(ui->cbx_src);
	gtk_widget_destroy(ui->spn_src_fact);
	gtkext_dial_destroy(ui->spn_compress);
	gtkext_dial_destroy(ui->spn_gattack);
	gtkext_dial_destroy(ui->spn_gdecay);
	gtkext_dial_destroy(ui->spn_gtarget);
	gtkext_dial_destroy(ui->spn_grms);
	gtk_widget_destroy(ui->cbx_lines);
	gtk_widget_destroy(ui->cbx_xfade);
	gtk_widget_destroy(ui->spn_psize);
	gtk_widget_destroy(ui->spn_vfreq);
	gtkext_dial_destroy(ui->spn_alpha);
	gtk_widget_destroy(ui->lbl_src_fact);
	gtk_widget_destroy(ui->lbl_psize);
	gtk_widget_destroy(ui->lbl_vfreq);
	gtk_widget_destroy(ui->lbl_compress);
	gtk_widget_destroy(ui->lbl_gattack);
	gtk_widget_destroy(ui->lbl_gdecay);
	gtk_widget_destroy(ui->lbl_gtarget);
	gtk_widget_destroy(ui->lbl_grms);
	gtk_widget_destroy(ui->sep_h0);
	gtk_widget_destroy(ui->sep_v0);

	delete ui->src;
	free(ui->scratch);
	free(ui->resampl);

	free(ui);
}

static void invalidate_gm(GMUI* ui) {
	gtk_widget_queue_draw_area(ui->m0, PC_BOUNDS, 0, GM_BOUNDS, GM_BOUNDS);
}

static void invalidate_pc(GMUI* ui) {
	float c;
	c = (PC_HEIGHT - PC_BLOCK) * ui->cor_u;
	gtk_widget_queue_draw_area(ui->m0, PC_LEFT, PC_TOP + c -1 , PC_WIDTH, PC_BLOCK + 2);
	ui->cor_u = ui->cor;
	c = (PC_HEIGHT - PC_BLOCK) * ui->cor_u;
	gtk_widget_queue_draw_area(ui->m0, PC_LEFT, PC_TOP + c -1 , PC_WIDTH, PC_BLOCK + 2);
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
	GMUI* ui = (GMUI*)handle;
	if (format != 0) return;
	if (port_index == 4) {
		float v = *(float *)buffer;
		if (v >= 0 && v <= 6.0) {
			ui->disable_signals = true;
			gtk_range_set_value(GTK_RANGE(ui->fader), v);
			ui->disable_signals = false;
		}
	} else
	if (port_index == 5) {
		ui->cor = 0.5f * (1.0f - *(float *)buffer);
		invalidate_pc(ui);
	} else
	if (port_index == 6) {
		ui->ntfy_b = (uint32_t) (*(float *)buffer);
		invalidate_gm(ui);
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
	"http://gareus.org/oss/lv2/meters#goniometerui",
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
