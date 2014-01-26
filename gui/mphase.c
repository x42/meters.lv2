/* Phase Wheel
 *
 * Copyright 2012-2014 Robin Gareus <robin@gareus.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#define MTR_URI "http://gareus.org/oss/lv2/meters#"
#define MTR_GUI "mphaseui"

#define NUM_BANDS 30

#define R_BAND 5

#define XOFF 5
#define YOFF 5

#define ANN_H 32
#define ANN_B 25

#define PC_BOUNDW ( 60.0f)
#define PC_BOUNDH (ui->height)

#define PC_TOP    (  5.0f)
#define PC_BLOCK  ( 10.0f)
#define PC_LEFT   ( 19.0f)
#define PC_WIDTH  ( 22.0f)
#define PC_HEIGHT (PC_BOUNDH - 2 * PC_TOP)
#define PC_BLOCKSIZE (PC_HEIGHT - PC_BLOCK)

typedef struct {
	RobWidget *rw;

	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	RobWidget* m0;
	RobWidget* m1;
	RobWidget* m2;

	RobWidget* hbox1;
	RobWidget* hbox2;
	RobTkDial* gain;

	cairo_surface_t* sf_dat;
	cairo_surface_t* sf_ann;

	PangoFontDescription *font[2];
	cairo_surface_t* sf_dial;
	cairo_surface_t* sf_gain;
	cairo_surface_t* sf_pc[2];

	float phase[NUM_BANDS];
	float peak[NUM_BANDS];
	float db_cutoff;
	float cor, cor_u;

	bool disable_signals;
	bool update_annotations;
	uint32_t width;
	uint32_t height;

	int32_t drag_cutoff_x;
	float   drag_cutoff_db;
	bool prelight_cutoff;

} MFUI;

/******************************************************************************
 * Drawing
 */

static void hsl2rgb(float c[3], const float hue, const float sat, const float lum) {
	const float cq = lum < 0.5 ? lum * (1 + sat) : lum + sat - lum * sat;
	const float cp = 2.f * lum - cq;
	c[0] = rtk_hue2rgb(cp, cq, hue + 1.f/3.f);
	c[1] = rtk_hue2rgb(cp, cq, hue);
	c[2] = rtk_hue2rgb(cp, cq, hue - 1.f/3.f);
}

static void create_surfaces(MFUI* ui) {
	cairo_t* cr;
	const double ccc = ui->width / 2.0 + .5;
	const double rad = (ui->width - XOFF) * .5;

	ui->sf_ann = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ui->width, ui->height);
	cr = cairo_create (ui->sf_ann);
	cairo_rectangle (cr, 0, 0, ui->width, ui->height);
	cairo_set_source_rgba(cr, .33, .33, .36, 1.0); // BG
	cairo_fill (cr);

	cairo_set_line_width (cr, 1.0);

	cairo_arc (cr, ccc, ccc, rad, 0, 2.0 * M_PI);
	cairo_set_source_rgba(cr, 0, 0, 0, 1.0);
	cairo_fill_preserve(cr);
	CairoSetSouerceRGBA(c_g90);
	cairo_stroke(cr);

	const double dash1[] = {1.0, 2.0};
	cairo_set_dash(cr, dash1, 2, 0);
	cairo_set_source_rgba(cr, .5, .5, .5, 1.0);

#define CIRC_ANN(RF, TXT) { \
	const float dr = R_BAND * RF; \
	cairo_arc (cr, ccc, ccc, dr, 0, 2.0 * M_PI); \
	cairo_stroke(cr); \
	const float px = ccc + dr * sinf(M_PI * -.75); \
	const float py = ccc - dr * cosf(M_PI * -.75); \
	write_text_full(cr, TXT, ui->font[0], px, py, M_PI * -.75, -2, c_g60); \
	}

	//CIRC_ANN(3.5, "50 Hz")
	CIRC_ANN(4.5, "63 Hz")
	CIRC_ANN(10.5, "250 Hz")
	CIRC_ANN(16.5, "1 KHz")
	//CIRC_ANN(20.5, "2.5 KHz")
	CIRC_ANN(22.5, "4 KHz")
	//CIRC_ANN(23.5, "5 KHz")
	//CIRC_ANN(26.5, "10 KHz")
	CIRC_ANN(28.5, "16 KHz")

	const double dash2[] = {1.0, 2.0};
	cairo_set_line_width(cr, 3.5);
	cairo_set_dash(cr, dash2, 2, 1);
	CairoSetSouerceRGBA(c_grb);


	cairo_set_line_width(cr, 1.5);
	cairo_move_to(cr, ccc - rad, ccc);
	cairo_line_to(cr, ccc + rad, ccc);
	cairo_stroke(cr);

	cairo_set_line_width(cr, 3.5);
	cairo_move_to(cr, ccc, ccc - rad);
	cairo_line_to(cr, ccc, ccc + rad);
	cairo_stroke(cr);
	cairo_set_dash(cr, NULL, 0, 0);

	write_text_full(cr, "+L",  ui->font[0], ccc, ccc - rad * .85, 0, -2, c_g60);
	write_text_full(cr, "-L",  ui->font[0], ccc, ccc + rad * .85, 0, -2, c_g60);
	write_text_full(cr, "0\u00B0",  ui->font[0], ccc, ccc - rad * .65, 0, -2, c_g60);
	write_text_full(cr, "180\u00B0",  ui->font[0], ccc, ccc + rad * .65, 0, -2, c_g60);

	write_text_full(cr, "-R",  ui->font[0], ccc - rad * .85, ccc, 0, -2, c_g60);
	write_text_full(cr, "+R",  ui->font[0], ccc + rad * .85, ccc, 0, -2, c_g60);
	write_text_full(cr, "-90\u00B0",  ui->font[0], ccc - rad * .65, ccc, 0, -2, c_g60);
	write_text_full(cr, "+90\u00B0",  ui->font[0], ccc + rad * .65, ccc, 0, -2, c_g60);


	ui->sf_dat = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ui->width, ui->height);
	cr = cairo_create (ui->sf_dat);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
	cairo_rectangle (cr, 0, 0, ui->width, ui->height);
	cairo_fill (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr, 0, 0, 0, 1.0);
	cairo_arc (cr, ccc, ccc, rad, 0, 2.0 * M_PI);
	cairo_fill (cr);
	cairo_destroy (cr);

	ui->sf_pc[0] = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, PC_WIDTH, 20);
	cr = cairo_create (ui->sf_pc[0]);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, .5, 0, .3);
	cairo_rectangle (cr, 0, 0, PC_WIDTH, 20);
	cairo_fill (cr);
	write_text_full(cr, "+1", ui->font[1], PC_WIDTH / 2, 10, 0, 2, c_g60);
	cairo_destroy (cr);

	ui->sf_pc[1] = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, PC_WIDTH, 20);
	cr = cairo_create (ui->sf_pc[1]);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, .5, 0, 0, .3);
	cairo_rectangle (cr, 0, 0, PC_WIDTH, 20);
	cairo_fill (cr);
	write_text_full(cr, "-1", ui->font[1], PC_WIDTH / 2, 10, 0, 2, c_g60);
	cairo_destroy (cr);

	ui->sf_gain = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ui->width, 40);

#define AMPLABEL(V, O, T, X) \
	{ \
		const float ang = (-.75 * M_PI) + (1.5 * M_PI) * ((V) + (O)) / (T); \
		xlp = X + .5 + sinf (ang) * (10 + 3.0); \
		ylp = 16.5 + .5 - cosf (ang) * (10 + 3.0); \
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); \
		CairoSetSouerceRGBA(c_wht); \
		cairo_set_line_width(cr, 1.5); \
		cairo_move_to(cr, rint(xlp)-.5, rint(ylp)-.5); \
		cairo_close_path(cr); \
		cairo_stroke(cr); \
		xlp = X + .5 + sinf (ang) * (10 + 9.5); \
		ylp = 16.5 + .5 - cosf (ang) * (10 + 9.5); \
	}

	ui->sf_dial = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 60, 40);
	cr = cairo_create (ui->sf_dial);
	float xlp, ylp;
	AMPLABEL(-30, 30., 60., 30.5); write_text_full(cr, "-30", ui->font[0], xlp, ylp, 0, 2, c_wht);
	AMPLABEL(-20, 30., 60., 30.5);
	AMPLABEL(-10, 30., 60., 30.5);
	AMPLABEL(  0, 30., 60., 30.5);
	AMPLABEL( 10, 30., 60., 30.5);
	AMPLABEL( 20, 30., 60., 30.5);
	AMPLABEL( 30, 30., 60., 30.5); write_text_full(cr, "+30", ui->font[0], xlp, ylp, 0, 2, c_wht); \
	cairo_destroy (cr);
}

static void update_annotations(MFUI* ui) {
	cairo_t* cr = cairo_create (ui->sf_gain);

	cairo_rectangle (cr, 0, 0, ui->width, 40);
	cairo_set_source_rgba(cr, .33, .33, .36, 1.0); // BG
	cairo_fill (cr);

	rounded_rectangle (cr, 3, 3 , ui->width - 6, ANN_H - 6, 6);
	if (ui->drag_cutoff_x >= 0 || ui->prelight_cutoff) {
		cairo_set_source_rgba(cr, .15, .15, .15, 1.0);
	} else {
		cairo_set_source_rgba(cr, .0, .0, .0, 1.0);
	}
	cairo_fill (cr);

	cairo_set_line_width (cr, 1.0);
	const uint32_t mxw = ui->width - XOFF * 2 - 36;
	const uint32_t mxo = XOFF + 18;
	for (uint32_t i=0; i < mxw; ++i) {

		float pk = i / (float)mxw;

		float clr[3];
		hsl2rgb(clr, .75 - .8 * pk, .9, .2 + pk * .4);
		cairo_set_source_rgba(cr, clr[0], clr[1], clr[2], 1.0);

		cairo_move_to(cr, mxo + i + .5, ANN_B - 5);
		cairo_line_to(cr, mxo + i + .5, ANN_B);
		cairo_stroke(cr);
	}

	cairo_set_source_rgba(cr, .8, .8, .8, .8);

#define DBTXT(DB, TXT) \
	write_text_full(cr, TXT, ui->font[0], mxo + rint(mxw * (60.0 + DB) / 60.0), ANN_B - 14 , 0, 2, c_wht); \
	cairo_move_to(cr, mxo + rint(mxw * (60.0 + DB) / 60.0) + .5, ANN_B - 7); \
	cairo_line_to(cr, mxo + rint(mxw * (60.0 + DB) / 60.0) + .5, ANN_B); \
	cairo_stroke(cr);

	const float gain = robtk_dial_get_value(ui->gain);
	for (int32_t db = -60; db <=0 ; db+= 10) {
		char dbt[16];
		if (db == 0) {
			snprintf(dbt, 16, "\u2265%+.0fdB", (db - gain));
		} else {
			snprintf(dbt, 16, "%+.0fdB", (db - gain));
		}
		DBTXT(db, dbt)
	}

	if (ui->db_cutoff > -59) {
		const float cox = rint(mxw * (ui->db_cutoff + 60.0)/ 60.0);
		if (ui->drag_cutoff_x >= 0 || ui->prelight_cutoff) {
			cairo_rectangle(cr, mxo, 6, cox, ANN_B - 6);
		} else {
			cairo_rectangle(cr, mxo, ANN_B - 6, cox, 7);
		}
		cairo_set_source_rgba(cr, .0, .0, .0, .7);
		cairo_fill(cr);

		cairo_set_line_width (cr, 1.0);
		cairo_set_source_rgba(cr, .9, .5, .5, .6);
		cairo_move_to(cr, mxo + cox + .5, ANN_B - 6);
		cairo_line_to(cr, mxo + cox + .5, ANN_B + 1);
		cairo_stroke(cr);
	}

	cairo_destroy (cr);
}

static void plot_data(MFUI* ui) {
	cairo_t* cr;
	const double ccc = ui->width / 2.0 + .5;
	const double rad = (ui->width - XOFF) * .5;
	cr = cairo_create (ui->sf_dat);
	cairo_arc (cr, ccc, ccc, rad, 0, 2.0 * M_PI);
	cairo_clip_preserve (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr, 0, 0, 0, .11);
	cairo_fill(cr);

	cairo_set_line_width (cr, 5.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	for (uint32_t i=0; i < NUM_BANDS ; ++i) {
		float dist = i * 5 + 2.5;
		float dx = ccc + dist * sinf(M_PI * ui->phase[i]);
		float dy = ccc - dist * cosf(M_PI * ui->phase[i]);
		if (ui->peak[i] < ui->db_cutoff) continue;
		float pk = (60 + ui->peak[i]) / 60.0;

		float clr[3];
		hsl2rgb(clr, .75 - .8 * pk, .9, .2 + pk * .4);

		cairo_set_source_rgba(cr, clr[0], clr[1], clr[2], 1.0);
		cairo_new_path (cr);
		cairo_move_to(cr, dx, dy);
		cairo_close_path(cr);
		cairo_stroke(cr);
	}

#if 1
	cairo_set_line_width (cr, 7.5);
	cairo_move_to(cr, ccc, ccc);
	float px = ccc;
	float py = ccc;
	float pp = -60;
	//float pa = 0;
	for (uint32_t i=0; i < NUM_BANDS ; ++i) {
		float dist = i * 5 + 2.5;
		float dx = ccc + dist * sinf(M_PI * ui->phase[i]);
		float dy = ccc - dist * cosf(M_PI * ui->phase[i]);

		if (ui->peak[i] < ui->db_cutoff || pp < ui->db_cutoff
			|| i == 0
			|| (px - dx) * (px - dx) + (py - dy) * (py - dy) > i * 50)
		{
			px = dx; py = dy; pp = ui->peak[i]; // pa = ui->phase[i];
			continue;
		}
		float pk = (120 + (ui->peak[i] + pp)) / 120.0;

		float clr[3];
		hsl2rgb(clr, .75 - .8 * pk, .9, .2 + pk * .4);

		cairo_set_source_rgba(cr, clr[0], clr[1], clr[2], 0.1);
		cairo_move_to(cr, px, py);
#if 1
		cairo_line_to(cr, dx, dy);
#else
		// spline control points
		float cp = (fmod(pa - ui->phase[i], 2.0)) / 3.0; // TODO wrap
		const float bzdt = dist - 2.5;
		const float c0x = ccc + bzdt * sinf(M_PI * (ui->phase[i] + cp * 2));
		const float c0y = ccc - bzdt * cosf(M_PI * (ui->phase[i] + cp * 2));
		const float c1x = ccc + bzdt * sinf(M_PI * (ui->phase[i] + cp));
		const float c1y = ccc - bzdt * cosf(M_PI * (ui->phase[i] + cp));
		cairo_curve_to(cr, c0x, c0y, c1x, c1y, dx, dy);
#endif
		cairo_stroke(cr);
		px = dx; py = dy; pp = ui->peak[i]; // pa = ui->phase[i];
	}
#endif

	cairo_destroy (cr);
}

static bool expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t *ev) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);

	plot_data(ui);

	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	cairo_set_source_surface(cr, ui->sf_ann, 0, 0);
	cairo_paint (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
	cairo_set_source_surface(cr, ui->sf_dat, 0, 0);
	cairo_paint (cr);

	return TRUE;
}

static bool ga_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t *ev) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);

	if (ui->update_annotations) {
		update_annotations(ui);
		ui->update_annotations = false;
	}

	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	cairo_set_source_surface(cr, ui->sf_gain, 0, 0);
	cairo_paint (cr);

	return TRUE;
}

static bool pc_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t *ev) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);

	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	/* display phase-correlation */
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	/* PC meter backgroud */
	cairo_set_source_rgba(cr, .33, .33, .36, 1.0); // BG
	cairo_rectangle (cr, 0, 0, PC_BOUNDW, PC_BOUNDH);
	cairo_fill(cr);

	CairoSetSouerceRGBA(c_blk);
	cairo_set_line_width(cr, 1.0);
	rounded_rectangle (cr, PC_LEFT, PC_TOP + 1.0, PC_WIDTH, PC_HEIGHT - 2.0, 6);
	cairo_fill_preserve(cr);
	cairo_save(cr);
	cairo_clip(cr);

	/* value */
	CairoSetSouerceRGBA(c_glb);
	const float c = rintf(PC_TOP + PC_BLOCKSIZE * ui->cor);
	rounded_rectangle (cr, PC_LEFT, c, PC_WIDTH, PC_BLOCK, 4);
	cairo_fill(cr);

	/* labels w/ background */
	cairo_set_source_surface(cr, ui->sf_pc[0], PC_LEFT, PC_TOP);
	cairo_paint (cr);
	cairo_set_source_surface(cr, ui->sf_pc[1], PC_LEFT, PC_TOP + PC_HEIGHT - 20);
	cairo_paint (cr);

	cairo_restore(cr);

	rounded_rectangle (cr, PC_LEFT - .5, PC_TOP + .5, PC_WIDTH + 1, PC_HEIGHT - 1, 6);
	CairoSetSouerceRGBA(c_g90);
	cairo_stroke(cr);

	/* annotations */
	cairo_set_operator (cr, CAIRO_OPERATOR_SCREEN);
	CairoSetSouerceRGBA(c_grb);
	cairo_set_line_width(cr, 1.0);

#define PC_ANNOTATION(YPOS, OFF) \
	cairo_move_to(cr, PC_LEFT + OFF, rintf(PC_TOP + YPOS) + 0.5); \
	cairo_line_to(cr, PC_LEFT + PC_WIDTH - OFF, rintf(PC_TOP + YPOS) + 0.5);\
	cairo_stroke(cr);

	PC_ANNOTATION(PC_HEIGHT * 0.1, 4.0);
	PC_ANNOTATION(PC_HEIGHT * 0.2, 4.0);
	PC_ANNOTATION(PC_HEIGHT * 0.3, 4.0);
	PC_ANNOTATION(PC_HEIGHT * 0.4, 4.0);
	PC_ANNOTATION(PC_HEIGHT * 0.6, 4.0);
	PC_ANNOTATION(PC_HEIGHT * 0.7, 4.0);
	PC_ANNOTATION(PC_HEIGHT * 0.8, 4.0);
	PC_ANNOTATION(PC_HEIGHT * 0.9, 4.0);

	CairoSetSouerceRGBA(c_glr);
	cairo_set_line_width(cr, 1.5);
	PC_ANNOTATION(PC_HEIGHT * 0.5, 1.5);

	return TRUE;
}


/******************************************************************************
 * UI callbacks  - Dial
 */

static bool cb_set_gain (RobWidget* handle, void *data) {
	MFUI* ui = (MFUI*) (data);
	ui->update_annotations = true;
	queue_draw(ui->m2);
	if (ui->disable_signals) return TRUE;
	float val = robtk_dial_get_value(ui->gain);
	ui->write(ui->controller, 61, sizeof(float), 0, (const void*) &val);
	return TRUE;
}

static void annotation_txt(MFUI *ui, RobTkDial * d, cairo_t *cr, const char *txt) {
	int tw, th;
	cairo_save(cr);
	PangoLayout * pl = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(pl, ui->font[1]);
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, &tw, &th);
	cairo_translate (cr, d->w_cx, d->w_height);
	cairo_translate (cr, -tw/2.0 - 0.5, -th);
	cairo_set_source_rgba (cr, .0, .0, .0, .7);
	rounded_rectangle(cr, -1, -1, tw+3, th+1, 3);
	cairo_fill(cr);
	CairoSetSouerceRGBA(c_wht);
	pango_cairo_layout_path(cr, pl);
	pango_cairo_show_layout(cr, pl);
	g_object_unref(pl);
	cairo_restore(cr);
	cairo_new_path(cr);
}

static void dial_annotation_db(RobTkDial * d, cairo_t *cr, void *data) {
	MFUI* ui = (MFUI*) (data);
	char tmp[16];
	snprintf(tmp, 16, "%+4.1fdB", d->cur);
	annotation_txt(ui, d, cr, tmp);
}

/******************************************************************************
 * UI callbacks  - Range
 */

static RobWidget* m2_mousedown(RobWidget* handle, RobTkBtnEvent *event) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	if (event->state & ROBTK_MOD_SHIFT) {
		ui->db_cutoff = -59;
		ui->update_annotations = true;
		queue_draw(ui->m2);
		return NULL;
	}

	ui->drag_cutoff_db = ui->db_cutoff;
	ui->drag_cutoff_x = event->x;

	ui->update_annotations = true;
	queue_draw(ui->m2);

	return handle;
}

static RobWidget* m2_mouseup(RobWidget* handle, RobTkBtnEvent *event) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	ui->drag_cutoff_x = -1;
	ui->update_annotations = true;
	queue_draw(ui->m2);
	return NULL;
}

static RobWidget* m2_mousemove(RobWidget* handle, RobTkBtnEvent *event) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	if (ui->drag_cutoff_x < 0) return NULL;
	const float mxw = 60. / (float) (ui->width - XOFF * 2 - 36);
	const float diff = (event->x - ui->drag_cutoff_x) * mxw;
	float cutoff = ui->drag_cutoff_db + diff;
	if (cutoff < -59) cutoff = -59;
	if (cutoff > -20) cutoff = -20;
	if (ui->db_cutoff != cutoff) {
		ui->db_cutoff = cutoff;
		ui->update_annotations = true;
		queue_draw(ui->m2);
		ui->write(ui->controller, 62, sizeof(float), 0, (const void*) &cutoff);
	}
	return handle;
}

static void m2_enter(RobWidget *handle) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	if (!ui->prelight_cutoff) {
		ui->prelight_cutoff = true;
		ui->update_annotations = true;
		queue_draw(ui->m2);
	}
}

static void m2_leave(RobWidget *handle) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	if (ui->prelight_cutoff) {
		ui->prelight_cutoff = false;
		ui->update_annotations = true;
		queue_draw(ui->m2);
	}
}


/******************************************************************************
 * widget hackery
 */

static void
size_request(RobWidget* handle, int *w, int *h) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	*w = ui->width;
	*h = ui->height;
}

static void
pc_size_request(RobWidget* handle, int *w, int *h) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	*w = PC_BOUNDW;
	*h = PC_BOUNDH;
}

static void
ga_size_request(RobWidget* handle, int *w, int *h) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	*w = ui->width;
	*h = ANN_H;
}

static RobWidget * toplevel(MFUI* ui, void * const top)
{
	/* main widget: layout */
	ui->rw = rob_vbox_new(FALSE, 0);
	robwidget_make_toplevel(ui->rw, top);

	ui->hbox1 = rob_hbox_new(FALSE, 0);
	ui->hbox2 = rob_hbox_new(FALSE, 0);

	rob_vbox_child_pack(ui->rw, ui->hbox1, TRUE, FALSE);
	rob_vbox_child_pack(ui->rw, ui->hbox2, TRUE, FALSE);


	ui->font[0] = pango_font_description_from_string("Mono 7");
	ui->font[1] = pango_font_description_from_string("Mono 8");
	create_surfaces(ui);

	/* main drawing area */
	ui->m0 = robwidget_new(ui);
	ROBWIDGET_SETNAME(ui->m0, "mphase (m0)");
	robwidget_set_expose_event(ui->m0, expose_event);
	robwidget_set_size_request(ui->m0, size_request);
	rob_hbox_child_pack(ui->hbox1, ui->m0, TRUE, FALSE);

	/* phase correlation */
	ui->m1 = robwidget_new(ui);
	ROBWIDGET_SETNAME(ui->m1, "phase (m1)");
	robwidget_set_expose_event(ui->m1, pc_expose_event);
	robwidget_set_size_request(ui->m1, pc_size_request);
	rob_hbox_child_pack(ui->hbox1, ui->m1, TRUE, FALSE);

	/* gain annotation */
	ui->m2 = robwidget_new(ui);
	ROBWIDGET_SETNAME(ui->m1, "gain (m2)");
	robwidget_set_expose_event(ui->m2, ga_expose_event);
	robwidget_set_size_request(ui->m2, ga_size_request);
	rob_hbox_child_pack(ui->hbox2, ui->m2, TRUE, FALSE);

	robwidget_set_mousedown(ui->m2, m2_mousedown);
	robwidget_set_mouseup(ui->m2, m2_mouseup);
	robwidget_set_mousemove(ui->m2, m2_mousemove);
	robwidget_set_enter_notify(ui->m2, m2_enter);
	robwidget_set_leave_notify(ui->m2, m2_leave);

	/* gain dial */
	ui->gain = robtk_dial_new_with_size(-30.0, 30.0, .01,
			60, 40, 30.5, 16.5, 10);
	robtk_dial_set_alignment(ui->gain, .5, 1.0);
	robtk_dial_set_value(ui->gain, 0);
	robtk_dial_set_default(ui->gain, 0);
	robtk_dial_set_callback(ui->gain, cb_set_gain, ui);
	robtk_dial_set_surface(ui->gain,ui->sf_dial);
	robtk_dial_annotation_callback(ui->gain, dial_annotation_db, ui);
	rob_hbox_child_pack(ui->hbox2, robtk_dial_widget(ui->gain), FALSE, FALSE);

	update_annotations(ui);
	return ui->rw;
}

/******************************************************************************
 * LV2 callbacks
 */

static void ui_enable(LV2UI_Handle handle) { }
static void ui_disable(LV2UI_Handle handle) { }

static LV2UI_Handle
instantiate(
		void* const               ui_toplevel,
		const LV2UI_Descriptor*   descriptor,
		const char*               plugin_uri,
		const char*               bundle_path,
		LV2UI_Write_Function      write_function,
		LV2UI_Controller          controller,
		RobWidget**               widget,
		const LV2_Feature* const* features)
{
	MFUI* ui = (MFUI*) calloc(1,sizeof(MFUI));
	*widget = NULL;

	if      (!strcmp(plugin_uri, MTR_URI "multiphase")) { ; }
	else if (!strcmp(plugin_uri, MTR_URI "multiphase_gtk")) { ; }
	else {
		free(ui);
		return NULL;
	}

	ui->write      = write_function;
	ui->controller = controller;

	for (uint32_t i=0; i < NUM_BANDS ; ++i) {
		ui->phase[i] = 0.0;
		ui->peak[i] = -60.0;
	}
	ui->db_cutoff = -59;
	ui->drag_cutoff_x = -1;
	ui->prelight_cutoff = false;
	ui->cor = ui->cor_u = 0.5;
	ui->disable_signals = false;
	ui->update_annotations = false;

	ui->width  = NUM_BANDS * 2 * R_BAND + 2 * XOFF;
	ui->height = NUM_BANDS * 2 * R_BAND + 2 * YOFF;

	*widget = toplevel(ui, ui_toplevel);

	return ui;
}

static enum LVGLResize
plugin_scale_mode(LV2UI_Handle handle)
{
	return LVGL_LAYOUT_TO_FIT;
}

static void
cleanup(LV2UI_Handle handle)
{
	MFUI* ui = (MFUI*)handle;

	pango_font_description_free(ui->font[0]);
	pango_font_description_free(ui->font[1]);

	cairo_surface_destroy(ui->sf_ann);
	cairo_surface_destroy(ui->sf_dat);
	cairo_surface_destroy(ui->sf_gain);
	cairo_surface_destroy(ui->sf_dial);
	cairo_surface_destroy(ui->sf_pc[0]);
	cairo_surface_destroy(ui->sf_pc[1]);

	robwidget_destroy(ui->m0);
	robwidget_destroy(ui->m1);
	robwidget_destroy(ui->m2);
	rob_box_destroy(ui->hbox1);
	rob_box_destroy(ui->hbox2);
	rob_box_destroy(ui->rw);

	free(ui);
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

/******************************************************************************
 * backend communication
 */

static void invalidate_pc(MFUI* ui, const float val) {
	float c;
	if (rint(PC_BLOCKSIZE * ui->cor_u * 2) == rint (PC_BLOCKSIZE * val * 2)) return;
	c = rintf(PC_TOP + PC_BLOCKSIZE * ui->cor_u);
	queue_tiny_area(ui->m1, PC_LEFT, c - 1 , PC_WIDTH, PC_BLOCK + 2);
	ui->cor_u = ui->cor = val;
	c = rintf(PC_TOP + PC_BLOCKSIZE * ui->cor_u);
	queue_tiny_area(ui->m1, PC_LEFT, c - 1 , PC_WIDTH, PC_BLOCK + 2);
}


static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	MFUI* ui = (MFUI*)handle;
	if (format != 0) return;

	if (port_index >= 0 && port_index < 30) {
		const int pidx = port_index;
		ui->phase[pidx] = *(float *)buffer;
		queue_draw(ui->m0);
	}
	else if (port_index >= 30 && port_index < 60) {
		const int pidx = port_index - 30;
		ui->peak[pidx] = *(float *)buffer;
		queue_draw(ui->m0);
	}
	else if (port_index == 60) {
		invalidate_pc(ui, 0.5f * (1.0f - *(float *)buffer));
	}
	else if (port_index == 61) {
		ui->disable_signals = true;
		robtk_dial_set_value(ui->gain, *(float *)buffer);
		ui->disable_signals = false;
	}
	else if (port_index == 62) {
		float val = *(float *)buffer;
		if (ui->drag_cutoff_x < 0 && val >= -59 && val <= -20) {
			ui->db_cutoff = val;
			ui->update_annotations = true;
			queue_draw(ui->m2);
		}
	}
}
