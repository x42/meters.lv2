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
	RobWidget* m0;

	bool redraw_labels;
	bool fontcache;
	PangoFontDescription *font[2];
	cairo_surface_t* sf_ann;

	uint32_t width;
	uint32_t height;
	bool update_grid;
	const char *nfo;

	float c_fg[4];
	float c_bg[4];

	/* current data */
	float lvl[8];
	uint8_t n_chn;

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

static void update_grid (SURui* ui) {
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

#if 1 // alpha
	PangoFontDescription *font = pango_font_description_from_string("Mono 32px");
	write_text_full (cr, "alpha-version", font, 0, 0, -.23, 2, c_g20);
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
	float coeff = powf(10, .05 * dB); \
	hsl2rgb(clr, .68 - .72 * coeff, .9, .3 + .4 * sqrt(coeff)); \
	float ypos = powf (10, .05 * dB); \
	sprintf (txt, "%d", dB); \
	cairo_arc (cr, 0, 0, ypos * rad, 0, 2.0 * M_PI); \
	cairo_set_source_rgba(cr, clr[0], clr[1], clr[2], 0.8); \
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
	ANNARC(-18);

	cairo_destroy (cr);
}

/******************************************************************************
 * Main drawing function
 */

static bool expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t *ev) {
	SURui* ui = (SURui*)GET_HANDLE(handle);

	if (ui->update_grid) {
		update_grid (ui);
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

	cairo_set_line_width (cr, 2.0);

	float x[8], y[8];
	for (uint32_t i = 0; i < ui->n_chn; ++i) {
		const float pk = ui->lvl[i];
		assert (pk >= 0 && pk <=1);

		const float ang = (float) i * 2.f * M_PI / (float) ui->n_chn;
		const float _sa = rad * sin (ang);
		const float _ca = rad * cos (ang);

		float clr[3];
		hsl2rgb(clr, .68 - .72 * pk, .9, .3 + .4 * sqrt(pk));
		cairo_set_source_rgba(cr, clr[0], clr[1], clr[2], 0.8);

		cairo_move_to (cr, 0, 0);
		cairo_line_to (cr, pk * _sa, -pk * _ca);
		cairo_stroke (cr);

		x[i] =  pk * _sa;
		y[i] = -pk * _ca;
	}

	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgba(cr, .7, .7, .7, .7);
#define DANG 0.35

	const float _tn = 1.0 / cos (DANG);
	for (uint32_t i = 0; i < ui->n_chn; ++i) {
		const int n = (i + 1 + ui->n_chn) % ui->n_chn;

		const float a0 =  DANG + (float)  i * 2.f * M_PI / (float) ui->n_chn;
		const float a1 = -DANG + (float)  n * 2.f * M_PI / (float) ui->n_chn;
		const float _sa0 = rad * sin (a0) * _tn;
		const float _ca0 = rad * cos (a0) * _tn;
		const float _sa1 = rad * sin (a1) * _tn;
		const float _ca1 = rad * cos (a1) * _tn;

		{
			const float r0 = ui->lvl[i];
			const float r1 = ui->lvl[n];
			cairo_move_to (cr, 0, 0);
			cairo_line_to (cr, x[i], y[i]);
			cairo_curve_to (cr,
					r0 * _sa0, -r0 * _ca0,
					r1 * _sa1, -r1 * _ca1,
					x[n], y[n]);
			cairo_close_path (cr);
		}
	}
	cairo_set_source_rgba(cr, .3, .3, .3, .8);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, .6, .6, .6, .8);
	cairo_stroke(cr);

	return TRUE;
}

/******************************************************************************
 * widget hackery
 */

static void
size_request(RobWidget* handle, int *w, int *h) {
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

	if      (!strcmp(plugin_uri, MTR_URI "surround8"))     { ui->n_chn = 8; }
	else if (!strcmp(plugin_uri, MTR_URI "surround8_gtk")) { ui->n_chn = 8; }
	else {
		free(ui);
		return NULL;
	}

	ui->nfo = robtk_info(ui_toplevel);
	ui->width  = 400;
	ui->height = 400;
	ui->update_grid = false;
	get_color_from_theme(0, ui->c_fg);
	get_color_from_theme(1, ui->c_bg);

	ui->box = rob_vbox_new(FALSE, 2);
	robwidget_make_toplevel(ui->box, ui_toplevel);
	ROBWIDGET_SETNAME(ui->box, "surmeter");

	/* main drawing area */
	ui->m0 = robwidget_new(ui);
	ROBWIDGET_SETNAME(ui->m0, "sur(m0)");
	robwidget_set_alignment(ui->m0, .5, .5);
	robwidget_set_expose_event(ui->m0, expose_event);
	robwidget_set_size_request(ui->m0, size_request);
	robwidget_set_size_allocate(ui->m0, m0_size_allocate);

	/* global packing */
	rob_vbox_child_pack(ui->box, ui->m0, TRUE, TRUE);

	*widget = ui->box;

	initialize_font_cache(ui);
	ui->redraw_labels = TRUE;

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
		for (int i=0; i < FONT_LAST; ++i) {
			pango_font_description_free(ui->font[i]);
		}
	}
	cairo_surface_destroy(ui->sf_ann);
	robwidget_destroy(ui->m0);
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
	} else if (port_index > 0 && port_index <= ui->n_chn * 3 && port_index %3 == 0) {
		float nl = *(float *)buffer; // meter_deflect(ui->type, *(float *)buffer);
		ui->lvl[(port_index - 1) / 3] = nl;
		queue_draw(ui->m0);
	}
}
