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

#define XOFF 3
#define YOFF 3
#define YANN 20

typedef struct {
	RobWidget *rw;

	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

  RobWidget* m0;

	cairo_surface_t* sf_dat;
	cairo_surface_t* sf_ann;

	float phase[NUM_BANDS];
	float peak[NUM_BANDS];

	bool disable_signals;
	uint32_t width;
	uint32_t height;

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


	ui->sf_ann = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ui->width, ui->height);
	cr = cairo_create (ui->sf_ann);
	cairo_rectangle (cr, 0, 0, ui->width, ui->height);
	cairo_set_source_rgba(cr, .33, .33, .36, 1.0); // BG
	cairo_fill (cr);

	cairo_set_line_width (cr, 1.0);

	cairo_arc (cr, ccc, ccc, rad, 0, 2.0 * M_PI);
	cairo_set_source_rgba(cr, 0, 0, 0, 1.0);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, .9, .9, .9, 1.0);
	cairo_stroke(cr);

	PangoFontDescription *font = pango_font_description_from_string("Mono 7");

	const double dash1[] = {1.0, 2.0};
	cairo_set_dash(cr, dash1, 2, 0);
	cairo_set_source_rgba(cr, .5, .5, .5, 1.0);

#define CIRC_ANN(RF, TXT) \
	write_text_full(cr, TXT, font, ccc - R_BAND * RF, ccc, M_PI * -.5, -2, c_wht); \
	cairo_arc (cr, ccc, ccc, R_BAND * RF, 0, 2.0 * M_PI); \
	cairo_stroke(cr);

	CIRC_ANN(3.5, "50Hz")
	CIRC_ANN(10.5, "250Hz")
	CIRC_ANN(16.5, "1KHz")
	CIRC_ANN(23.5, "5KHz")
	CIRC_ANN(26.5, "10KHz")

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

	write_text_full(cr, "0",  font, ccc, ccc - rad * .82, 0, -2, c_wht);
	write_text_full(cr, "-1", font, ccc - 10, ccc + rad * .82, 0, -2, c_wht);
	write_text_full(cr, "+1", font, ccc + 10, ccc + rad * .82, 0, -2, c_wht);

	cairo_set_line_width (cr, 1.0);
	const uint32_t mxw = ui->width - XOFF * 2 - 20;
	const uint32_t mxo = XOFF + 10;
	for (uint32_t i=0; i < mxw; ++i) {

		float pk = i / (float)mxw;

		float clr[3];
		hsl2rgb(clr, .75 - .8 * pk, .9, .2 + pk * .4);
		cairo_set_source_rgba(cr, clr[0], clr[1], clr[2], 1.0);

		cairo_move_to(cr, mxo + i + .5, ui->height - YOFF - 5);
		cairo_line_to(cr, mxo + i + .5, ui->height - YOFF);
		cairo_stroke(cr);
	}

	cairo_set_source_rgba(cr, .8, .8, .8, .8);

#define DBTXT(DB, TXT) \
	write_text_full(cr, TXT, font, mxo + rint(mxw * (60.0 + DB) / 60.0), ui->height - YOFF - 12 , 0, 2, c_wht); \
	cairo_move_to(cr, mxo + rint(mxw * (60.0 + DB) / 60.0) + .5, ui->height - YOFF - 7); \
	cairo_line_to(cr, mxo + rint(mxw * (60.0 + DB) / 60.0) + .5, ui->height - YOFF); \
	cairo_stroke(cr);

	DBTXT(-60, "-60dB")
	DBTXT(-50, "-50dB")
	DBTXT(-40, "-40dB")
	DBTXT(-30, "-30dB")
	DBTXT(-20, "-20dB")
	DBTXT(-10, "-10dB")
	DBTXT(  0, "0dBFS")

	pango_font_description_free(font);
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
	cairo_set_source_rgba(cr, 0, 0, 0, .1);
	cairo_fill(cr);

	cairo_set_line_width (cr, 4.5);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	for (uint32_t i=0; i < NUM_BANDS ; ++i) {
		float dist = i * 5 + 2.5;
		float dx = ccc + dist * sinf(M_PI * ui->phase[i]);
		float dy = ccc - dist * cosf(M_PI * ui->phase[i]);
		float clr[3];
		float pk = (60 + ui->peak[i]) / 60.0;

		hsl2rgb(clr, .75 - .8 * pk, .9, .2 + pk * .4);

		cairo_set_source_rgba(cr, clr[0], clr[1], clr[2], 1.0);
		cairo_new_path (cr);
		cairo_move_to(cr, dx, dy);
		cairo_close_path(cr);
		cairo_stroke(cr);
	}
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

/******************************************************************************
 * UI callbacks
 */

/******************************************************************************
 * widget hackery
 */

static void
size_request(RobWidget* handle, int *w, int *h) {
	MFUI* ui = (MFUI*)GET_HANDLE(handle);
	*w = ui->width;
	*h = ui->height;
}

static RobWidget * toplevel(MFUI* ui, void * const top)
{
	/* main widget: layout */
	ui->rw = rob_hbox_new(FALSE, 2);
	robwidget_make_toplevel(ui->rw, top);

	/* main drawing area */
	ui->m0 = robwidget_new(ui);
	ROBWIDGET_SETNAME(ui->m0, "mphase (m0)");

	robwidget_set_expose_event(ui->m0, expose_event);
	robwidget_set_size_request(ui->m0, size_request);

	rob_hbox_child_pack(ui->rw, ui->m0, TRUE, FALSE);

	create_surfaces(ui);

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
	ui->disable_signals = false;

	ui->width = NUM_BANDS * 2 * R_BAND +  2 * XOFF;
	ui->height = ui->width + 2 * YOFF + YANN;

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

	robwidget_destroy(ui->m0);
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
}
