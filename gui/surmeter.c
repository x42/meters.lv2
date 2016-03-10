/* surround meter UI
 *
 * Copyright 2016 Robin Gareus <robin@gareus.org>
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

#define LVGL_RESIZEABLE

#define RTK_URI "http://gareus.org/oss/lv2/meters#"
#define RTK_GUI "surmeterui"
#define MTR_URI RTK_URI

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#ifndef MIN
#define MIN(A,B) ( (A) < (B) ? (A) : (B) )
#endif
#ifndef MAX
#define MAX(A,B) ( (A) > (B) ? (A) : (B) )
#endif

/*************************/
enum {
	FONT_M10 = 0,
	FONT_S10,
	FONT_LAST,
};

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	RobWidget* box;
	RobWidget* tbl;
	RobTkSep*  sep_h0;

	bool disable_signals;
	bool fontcache;
	PangoFontDescription *font[2];
	float c_fg[4];
	float c_bg[4];

	/* main drawing */
	RobWidget*       m0;
	uint32_t         width;
	uint32_t         height;
	cairo_surface_t* sf_ann;
	cairo_pattern_t* pat;
	bool             update_grid;

	/* correlation pairs UI*/
	RobWidget*       m_cor[4];
	uint32_t         cor_w;
	uint32_t         cor_h;
	cairo_surface_t* sf_cor;
	bool             update_cor;
	RobTkLbl*        lbl_cor[3];
	RobTkSelect*     sel_cor_a[4];
	RobTkSelect*     sel_cor_b[4];

	/* current data */
	float peak[8];
	float rms[8];
	float cor[4];

	/* settings */
	uint8_t n_chn;
	const char *nfo;
} SURui;


/******************************************************************************
 * custom visuals
 */

#define FONT(A) ui->font[(A)]

static void initialize_font_cache(SURui* ui) {
	ui->fontcache = true;
	ui->font[FONT_M10] = pango_font_description_from_string("Mono 10px");
	ui->font[FONT_S10] = pango_font_description_from_string("Sans 10px");
	assert(ui->font[FONT_M10]);
	assert(ui->font[FONT_S10]);
}

/******************************************************************************
 * Helpers for Drawing
 */

static void speaker(SURui* ui, cairo_t* cr, uint32_t n) {
	cairo_save (cr);
	cairo_rotate (cr, M_PI * -.5);
	cairo_scale (cr, 1.5, 1.5);
	cairo_move_to     (cr,  2, -.5);
	cairo_rel_line_to (cr,  0, -7);
	cairo_rel_line_to (cr,  5,  5);
	cairo_rel_line_to (cr,  5,  0);
	cairo_rel_line_to (cr,  0,  5);
	cairo_rel_line_to (cr, -5,  0);
	cairo_rel_line_to (cr, -5,  5);
	cairo_rel_line_to (cr,  0, -7);
	cairo_close_path (cr);

	char txt[16];
	CairoSetSouerceRGBA (c_g60);
	cairo_fill (cr);
	cairo_restore (cr);

	sprintf (txt,"%d", n);
	write_text_full (cr, txt, FONT(FONT_S10), 0, -10, 0, 2, ui->c_bg);
}

static void hsl2rgb(float c[3], const float hue, const float sat, const float lum) {
	const float cq = lum < 0.5 ? lum * (1 + sat) : lum + sat - lum * sat;
	const float cp = 2.f * lum - cq;
	c[0] = rtk_hue2rgb(cp, cq, hue + 1.f/3.f);
	c[1] = rtk_hue2rgb(cp, cq, hue);
	c[2] = rtk_hue2rgb(cp, cq, hue - 1.f/3.f);
}

static float meter_deflect (const float coeff) {
	return sqrtf (coeff);
}

static float db_deflect (const float dB) {
	return meter_deflect (powf (10, .05 * dB));
}

static void draw_grid (SURui* ui) {
	if (ui->sf_ann) cairo_surface_destroy(ui->sf_ann);
	ui->sf_ann = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ui->width, ui->height);
	cairo_t* cr = cairo_create (ui->sf_ann);

	const double mwh = MIN (ui->width, ui->height) *.9;
	const double rad = rint (mwh / 2.0) + .5;
	const double ccx = rint (ui->width / 2.0) + .5;
	const double ccy = rint (ui->height / 2.0) + .5;

	cairo_rectangle (cr, 0, 0, ui->width, ui->height);
	CairoSetSouerceRGBA(ui->c_bg);
	cairo_fill (cr);

	cairo_set_line_width (cr, 1.0);

	cairo_arc (cr, ccx, ccy, rad, 0, 2.0 * M_PI);
	cairo_set_source_rgba(cr, 0, 0, 0, 1.0);
	cairo_fill_preserve (cr);
	CairoSetSouerceRGBA (c_g90);
	cairo_stroke (cr);

	cairo_translate (cr, ccx, ccy);

#if 1 // beta
	PangoFontDescription *font = pango_font_description_from_string("Mono 32px");
	write_text_full (cr, "beta-version", font, 0, 0, -.23, 2, c_g20);
	pango_font_description_free(font);
#endif

	float sc = mwh / 360.0;
	for (uint32_t i = 0; i < ui->n_chn; ++i) {
		cairo_save (cr);
		cairo_rotate (cr, (float) i * 2.0 * M_PI / (float) ui->n_chn);
		cairo_translate (cr, 0, -rad);

		cairo_scale (cr, sc, sc);
		speaker (ui, cr, i + 1);
		cairo_restore (cr);
	}

	const double dash2[] = {1.0, 3.0};
	cairo_set_dash(cr, dash2, 2, 2);
	cairo_set_operator (cr, CAIRO_OPERATOR_ADD);

#define ANNARC(dB) { \
	char txt[16]; \
	float clr[3]; \
	float coeff = db_deflect (dB); \
	hsl2rgb(clr, .68 - .72 * coeff, .9, .3 + .4 * sqrt(coeff)); \
	float ypos = coeff; \
	sprintf (txt, "%d", dB); \
	cairo_arc (cr, 0, 0, ypos * rad, 0, 2.0 * M_PI); \
	cairo_set_source_rgba(cr, clr[0], clr[1], clr[2], 1.0); \
	cairo_stroke (cr); \
	cairo_save (cr); \
	cairo_rotate (cr, M_PI / 4.0); \
	cairo_scale (cr, sc, sc); \
	write_text_full (cr, txt, FONT(FONT_S10), 0, ypos * rad / sc, M_PI, 2, ui->c_fg); \
	cairo_restore (cr); \
}

	ANNARC(-3);
	ANNARC(-6);
	ANNARC(-9);
	ANNARC(-13);
	ANNARC(-18);
	ANNARC(-24);
	ANNARC(-36);

	cairo_destroy (cr);

	cairo_pattern_t* pat = cairo_pattern_create_radial (0, 0, 0, 0, 0, rad);

	cairo_pattern_add_color_stop_rgba(pat, 0.0,            .05, .05, .05, 0.7);

	cairo_pattern_add_color_stop_rgba(pat, db_deflect(-24.5), .0, .0, .8, 0.7);
	cairo_pattern_add_color_stop_rgba(pat, db_deflect(-23.5), .0, .5, .4, 0.7);

	cairo_pattern_add_color_stop_rgba(pat, db_deflect( -9.5), .0, .6, .0, 0.7);
	cairo_pattern_add_color_stop_rgba(pat, db_deflect( -8.5), .0, .8, .0, 0.7);

	cairo_pattern_add_color_stop_rgba(pat, db_deflect( -6.5), .1, .9, .1, 0.7);
	cairo_pattern_add_color_stop_rgba(pat, db_deflect( -5.5), .5, .9, .0, 0.7);

	cairo_pattern_add_color_stop_rgba(pat, db_deflect( -3.5), 75,.75, .0, 0.7);
	cairo_pattern_add_color_stop_rgba(pat, db_deflect( -2.5), .8, .4, .1, 0.7);

	cairo_pattern_add_color_stop_rgba(pat, db_deflect( -1.5), .9, .0, .0, 0.7);
	cairo_pattern_add_color_stop_rgba(pat, db_deflect( -0.5),  1, .1, .0, 0.7);
	cairo_pattern_add_color_stop_rgba(pat, 1.0 ,               1, .1, .0, 0.7);

	if (ui->pat) cairo_pattern_destroy(ui->pat);
	ui->pat= pat;
}

static void draw_cor (SURui* ui) {
	if (ui->sf_cor) cairo_surface_destroy(ui->sf_cor);
	ui->sf_cor = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ui->cor_w, ui->cor_h);
	cairo_t* cr = cairo_create (ui->sf_cor);

	cairo_rectangle (cr, 0, 0, ui->cor_w, ui->cor_h);
	CairoSetSouerceRGBA(ui->c_bg);
	cairo_fill (cr);

	rounded_rectangle (cr, 4, 4, ui->cor_w - 8, ui->cor_h - 8, 5);
	CairoSetSouerceRGBA(c_g60);
	cairo_stroke_preserve (cr);
	CairoSetSouerceRGBA(c_g30);
	cairo_fill_preserve (cr);
	cairo_clip (cr);

	CairoSetSouerceRGBA(c_g60);
	cairo_set_line_width (cr, 1.0);

	const double dash2[] = {1.0, 2.0};
	cairo_set_dash(cr, dash2, 2, 2);

	for (uint32_t i = 1; i < 10; ++i) {
		if (i == 5) continue;
		const float px = 10.5f + rint ((ui->cor_w - 20.f) * i / 10.f);
		cairo_move_to (cr, px, 5);
		cairo_line_to (cr, px, ui->cor_h - 5);
		cairo_stroke (cr);
	}

	write_text_full (cr, "-1", FONT(FONT_S10), 8, ui->cor_h * .5, 0, 3, ui->c_fg);
	write_text_full (cr,  "0", FONT(FONT_S10), rintf(ui->cor_w *.5), ui->cor_h * .5, 0, 2, ui->c_fg);
	write_text_full (cr, "+1", FONT(FONT_S10), ui->cor_w - 8.f, ui->cor_h * .5, 0, 1, ui->c_fg);
	cairo_destroy (cr);
}

/******************************************************************************
 * Main drawing function
 */

static bool
m0_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t *ev) {
	SURui* ui = (SURui*)GET_HANDLE(handle);

	if (ui->update_grid) {
		draw_grid (ui);
		ui->update_grid = false;
	}

	cairo_rectangle (cr, 0, 0, ui->width, ui->height);
	cairo_clip (cr);
	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	cairo_set_source_surface(cr, ui->sf_ann, 0, 0);
	cairo_paint (cr);

	const double mwh = MIN (ui->width, ui->height) - 55.0;
	const double rad = rint (mwh / 2.0) + .5;
	const double ccx = rint (ui->width / 2.0) + .5;
	const double ccy = rint (ui->height / 2.0) + .5;

	cairo_arc (cr, ccx, ccy, rad, 0, 2.0 * M_PI);
	cairo_clip (cr);

	//cairo_set_operator (cr, CAIRO_OPERATOR_SCREEN);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_translate (cr, ccx, ccy);

	// pre-calculate angles
	float x[8], y[8];
	for (uint32_t i = 0; i < ui->n_chn; ++i) {
		const float ang = (float) i * 2.f * M_PI / (float) ui->n_chn;
		x[i] = rad * sin (ang);
		y[i] = rad * cos (ang);
	}

	// tangential (for splines)
	const float d_ang = MIN (.6, M_PI / (ui->n_chn + 1.f));
	const float _tn = 1.0 / cos (d_ang);

	for (uint32_t i = 0; i < ui->n_chn; ++i) {
		const int n = (i + 1 + ui->n_chn) % ui->n_chn;
		const float pk0 = ui->rms[i];
		const float pk1 = ui->rms[n];

		const float a0 =  d_ang + (float)  i * 2.f * M_PI / (float) ui->n_chn;
		const float a1 = -d_ang + (float)  n * 2.f * M_PI / (float) ui->n_chn;
		const float _sa0 = rad * sin (a0) * _tn;
		const float _ca0 = rad * cos (a0) * _tn;
		const float _sa1 = rad * sin (a1) * _tn;
		const float _ca1 = rad * cos (a1) * _tn;

		{
			const float r0 = ui->rms[i];
			const float r1 = ui->rms[n];
			cairo_move_to (cr, 0, 0);
			cairo_line_to (cr, pk0 * x[i], -pk0 * y[i]);
			cairo_curve_to (cr,
					r0 * _sa0, -r0 * _ca0,
					r1 * _sa1, -r1 * _ca1,
					pk1 * x[n], -pk1 * y[n]);
			cairo_close_path (cr);
		}
	}

	cairo_set_source (cr, ui->pat);
	cairo_fill_preserve(cr);
	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgba(cr, .6, .6, .6, .8);
	cairo_stroke(cr);

	float lw = ceilf (5.f * rad / 200.f);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	for (uint32_t i = 0; i < ui->n_chn; ++i) {
		float pk = ui->peak[i];
		if (pk > 1) pk = 1.0;

		cairo_move_to (cr, 0, 0);
		cairo_line_to (cr, pk * x[i], -pk * y[i]);

		cairo_set_line_width (cr, lw);
		cairo_set_source_rgba(cr, .1, .1, .1, .8);
		cairo_stroke_preserve (cr);
		cairo_set_source (cr, ui->pat);
		cairo_set_line_width (cr, lw - 2.f);
		cairo_stroke (cr);
	}

	return TRUE;
}


static bool
cor_expose_event(RobWidget* rw, cairo_t* cr, cairo_rectangle_t *ev) {
	SURui* ui = (SURui*)GET_HANDLE(rw);

	if (ui->update_cor) {
		draw_cor (ui);
		ui->update_cor = false;
	}

	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	cairo_set_source_surface(cr, ui->sf_cor, 0, 0);
	cairo_paint (cr);

	const float ww = rw->area.width;
	const float hh = rw->area.height;

	rounded_rectangle (cr, 4, 4, ww - 8, hh - 8, 6);
	cairo_clip (cr);

	uint32_t pn = ui->n_chn;
	for (uint32_t cc = 0; cc < ui->n_chn; ++cc) {
		if (rw == ui->m_cor[cc]) { pn = cc; break; }
	}

	if (pn == ui->n_chn) {
		return TRUE;
	}

	cairo_set_line_width (cr, 13.0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

	const float px = 10.5f + (ww - 20.f) * ui->cor[pn];
	cairo_move_to (cr, px, 9);
	cairo_line_to (cr, px, hh - 9);

	if (ui->cor[pn] < .35) {
		cairo_set_source_rgba(cr, .8, .1, .1, .7);
	} else if (ui->cor[pn] < .65) {
		cairo_set_source_rgba(cr, .75, .75, 0, .7);
	} else {
		cairo_set_source_rgba(cr, .1, .8, .1, .7);
	}
	cairo_stroke(cr);

	return TRUE;
}

/******************************************************************************
 * UI callbacks
 */
static bool cb_set_port (RobWidget* rw, void *data) {
	SURui* ui = (SURui*)(data);
	if (ui->disable_signals) return TRUE;

	uint32_t pn = 0;
	for (uint32_t cc = 0; cc < ui->n_chn; ++cc) {
		if (rw->self == ui->sel_cor_a[cc]) { pn = 1 + cc * 3; break; }
		if (rw->self == ui->sel_cor_b[cc]) { pn = 2 + cc * 3; break; }
	}
	const float pv = robtk_select_get_value((RobTkSelect*)rw->self);
	if (pn > 0) {
		ui->write(ui->controller, pn, sizeof(float), 0, (const void*) &pv);
	}
	return TRUE;
}


/******************************************************************************
 * widget sizes
 */

static void
m0_size_request(RobWidget* handle, int *w, int *h) {
	*w = 400;
	*h = 400;
}

static void
m0_size_allocate(RobWidget* rw, int w, int h) {
	SURui* ui = (SURui*)GET_HANDLE(rw);
	ui->width = w;
	ui->height = h;
	ui->update_grid = true;
	robwidget_set_size(rw, w, h);
	queue_draw(rw);
}

static void
cor_size_request(RobWidget* handle, int *w, int *h) {
	*w = 80;
	*h = 28;
}

static void
cor_size_allocate(RobWidget* rw, int w, int h) {
	SURui* ui = (SURui*)GET_HANDLE(rw);
	ui->cor_w = w;
	ui->cor_h = h;
	ui->update_cor = true;
	robwidget_set_size(rw, w, h);
	queue_draw(rw);
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
	SURui* ui = (SURui*)calloc(1,sizeof(SURui));
	ui->write      = write_function;
	ui->controller = controller;

	*widget = NULL;

	if      (!strcmp(plugin_uri, MTR_URI "surround8")) { ui->n_chn = 8; }
	else if (!strcmp(plugin_uri, MTR_URI "surround5")) { ui->n_chn = 5; }
	else {
		free(ui);
		return NULL;
	}

	ui->nfo = robtk_info(ui_toplevel);
	ui->width  = 400;
	ui->height = 400;
	ui->update_grid = true;
	ui->update_cor = false;
	get_color_from_theme(0, ui->c_fg);
	get_color_from_theme(1, ui->c_bg);

	ui->box = rob_vbox_new(FALSE, 2);
	robwidget_make_toplevel(ui->box, ui_toplevel);
	ROBWIDGET_SETNAME(ui->box, "surmeter");

	ui->tbl = rob_table_new (/*rows*/7, /*cols*/3, FALSE);
	ROBWIDGET_SETNAME(ui->tbl, "surlayout");

	/* main drawing area */
	ui->m0 = robwidget_new(ui);
	ROBWIDGET_SETNAME(ui->m0, "sur(m0)");
	robwidget_set_alignment(ui->m0, .5, .5);
	robwidget_set_expose_event(ui->m0, m0_expose_event);
	robwidget_set_size_request(ui->m0, m0_size_request);
	robwidget_set_size_allocate(ui->m0, m0_size_allocate);
	rob_table_attach_defaults (ui->tbl, ui->m0, 1, 2, 0, 1);

	ui->sep_h0 = robtk_sep_new (TRUE);
	rob_table_attach (ui->tbl, robtk_sep_widget(ui->sep_h0),     0, 3, 1, 2, 0, 8, RTK_EXANDF, RTK_SHRINK);

	/* correlation headings */
	ui->lbl_cor[0]  = robtk_lbl_new("Stereo Pair Correlation");
	ui->lbl_cor[1]  = robtk_lbl_new("Chn");
	ui->lbl_cor[2]  = robtk_lbl_new("Chn");
	rob_table_attach (ui->tbl, robtk_lbl_widget(ui->lbl_cor[1]), 0, 1, 2, 3, 0, 0, RTK_SHRINK, RTK_SHRINK);
	rob_table_attach (ui->tbl, robtk_lbl_widget(ui->lbl_cor[0]), 1, 2, 2, 3, 0, 0, RTK_EXANDF, RTK_SHRINK);
	rob_table_attach (ui->tbl, robtk_lbl_widget(ui->lbl_cor[2]), 2, 3, 2, 3, 0, 0, RTK_SHRINK, RTK_SHRINK);

	/* correlation pairs */
	for (uint32_t c = 0; c < 4; ++c) {
		ui->sel_cor_a[c] = robtk_select_new();
		ui->sel_cor_b[c] = robtk_select_new();
		for (uint32_t cc = 0; cc < ui->n_chn; ++cc) {
			char tmp[8];
			sprintf (tmp, "%d", cc + 1);
			robtk_select_add_item(ui->sel_cor_a[c], cc, tmp);
			robtk_select_add_item(ui->sel_cor_b[c], cc, tmp);
			robtk_select_set_default_item(ui->sel_cor_a[c], (2 * c) % ui->n_chn); // XXX
			robtk_select_set_default_item(ui->sel_cor_b[c], (2 * c + 1) % ui->n_chn); // XXX
			robtk_select_set_callback (ui->sel_cor_a[c], cb_set_port, ui);
			robtk_select_set_callback (ui->sel_cor_b[c], cb_set_port, ui);
		}

		/* correlation display area */
		ui->m_cor[c] = robwidget_new(ui);
		ROBWIDGET_SETNAME(ui->m_cor[c], "cor");
		robwidget_set_alignment(ui->m_cor[c], .5, .5);
		robwidget_set_expose_event(ui->m_cor[c], cor_expose_event);
		robwidget_set_size_request(ui->m_cor[c], cor_size_request);
		robwidget_set_size_allocate(ui->m_cor[c], cor_size_allocate);

		rob_table_attach (ui->tbl, robtk_select_widget(ui->sel_cor_a[c]), 0, 1, c + 3, c + 4, 0, 0, RTK_SHRINK, RTK_SHRINK);
		rob_table_attach_defaults (ui->tbl, ui->m_cor[c],                 1, 2, c + 3, c + 4);
		rob_table_attach (ui->tbl, robtk_select_widget(ui->sel_cor_b[c]), 2, 3, c + 3, c + 4, 0, 0, RTK_SHRINK, RTK_SHRINK );
	}

	/* global packing */
	rob_vbox_child_pack(ui->box, ui->tbl, TRUE, TRUE);

	*widget = ui->box;

	initialize_font_cache(ui);
	ui->disable_signals = false;

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
	SURui* ui = (SURui*)handle;

	if (ui->fontcache) {
		for (uint32_t i = 0; i < FONT_LAST; ++i) {
			pango_font_description_free(ui->font[i]);
		}
	}
	for (uint32_t i = 0; i < 4; ++i) {
			robtk_select_destroy (ui->sel_cor_a[i]);
			robtk_select_destroy (ui->sel_cor_b[i]);
			robwidget_destroy(ui->m_cor[i]);
	}
	robtk_lbl_destroy(ui->lbl_cor[0]);
	robtk_lbl_destroy(ui->lbl_cor[1]);
	robtk_lbl_destroy(ui->lbl_cor[2]);
	robtk_sep_destroy(ui->sep_h0);
	cairo_surface_destroy(ui->sf_ann);
	cairo_surface_destroy(ui->sf_cor);
	cairo_pattern_destroy(ui->pat);
	robwidget_destroy(ui->m0);
	rob_table_destroy (ui->tbl);
	rob_box_destroy(ui->box);
	free(ui);
}

static const void*
extension_data(const char* uri)
{
	return NULL;
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
	SURui* ui = (SURui*)handle;

	if ( format != 0 ) { return; }

	if (port_index == 0) {
		// callib
	} else if (port_index > 0 && port_index <= 12 && port_index % 3 == 0) {
		// correlation data
		const uint32_t cc = (port_index - 1) / 3;
		ui->cor[cc] = .5f * (1.0f + *(float *)buffer);
		queue_draw (ui->m_cor[cc]);
	} else if (port_index > 0 && port_index <= 12 && port_index % 3 == 1) {
		// correlation input A
		const uint32_t cc = (port_index - 1) / 3;
		uint32_t pn = rintf(*(float *)buffer);
		ui->disable_signals = true;
		robtk_select_set_value(ui->sel_cor_a[cc], pn);
		ui->disable_signals = false;
	} else if (port_index > 0 && port_index <= 12 && port_index % 3 == 2) {
		// correlation input B
		const uint32_t cc = (port_index - 1) / 3;
		uint32_t pn = rintf(*(float *)buffer);
		ui->disable_signals = true;
		robtk_select_set_value(ui->sel_cor_b[cc], pn);
		ui->disable_signals = false;
	} else if (port_index > 12 && port_index <= 12U + ui->n_chn * 4 && port_index % 4 == 3) {
		ui->rms[(port_index - 13) / 4] = meter_deflect(*(float *)buffer);
		queue_draw (ui->m0);
	} else if (port_index > 12 && port_index <= 12U + ui->n_chn * 4 && port_index % 4 == 0) {
		ui->peak[(port_index - 13) / 4] = meter_deflect(*(float *)buffer);
		queue_draw (ui->m0);
	}
}
