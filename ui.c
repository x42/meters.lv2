/* meters.lv2
 *
 * Copyright (C) 2013 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2008-2012 Fons Adriaensen <fons@linuxaudio.org>
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
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <pango/pango.h>

#include "img/vu.c"
#include "img/bbc.c"
#include "img/din.c"
#include "img/ebu.c"
#include "img/nor.c"
#include "img/cor.c"
#include "img/screw.c"

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#define MTR_URI "http://gareus.org/oss/lv2/meters#"

/* meter types */
enum MtrType {
	MT_BBC = 1,
	MT_EBU,
	MT_DIN,
	MT_NOR,
	MT_VU,
	MT_COR
};

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	GtkWidget* box;
	GtkWidget* m0;

	cairo_surface_t * bg;
	cairo_surface_t * adj;
	unsigned char   * img0;
	unsigned char   * img1;
	float col[3];

	float lvl[2];
	float cal;
	float cal_rad;
	int chn;
	enum MtrType type;

	float drag_x, drag_y, drag_cal;
} MetersLV2UI;

struct MyGimpImage {
	unsigned int   width;
	unsigned int   height;
	unsigned int   bytes_per_pixel;
	unsigned char  pixel_data[];
};

/* screw area */
static const float s_xc = 150; // was (300.0 * ui->chn)/2.0;
static const float s_yc = 153;
static const float s_w2 = 12.5;
static const float s_h2 = 12.5;

/* colors */
static const float c_red[3] = {1.0, 0.0, 0.0};
static const float c_grn[3] = {0.0, 1.0, 0.0};
static const float c_blk[3] = {0.0, 0.0, 0.0};
static const float c_wht[3] = {1.0, 1.0, 1.0};


/* load gimp-exported .c image into cairo surface */
static void img2surf (struct MyGimpImage const * img, cairo_surface_t **s, unsigned char **d) {
	int x,y;
	int stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, img->width);

	(*d) = malloc (stride * img->height);
	(*s) = cairo_image_surface_create_for_data(*d,
			CAIRO_FORMAT_ARGB32, img->width, img->height, stride);

	for (y = 0; y < img->height; ++y) {
		const int y0 = y * stride;
		const int ys = y * img->width * img->bytes_per_pixel;
		for (x = 0; x < img->width; ++x) {
			const int xs = x * img->bytes_per_pixel;
			const int xd = x * 4;

			if (img->bytes_per_pixel == 3) {
			(*d)[y0 + xd + 3] = 0xff;
			} else {
			(*d)[y0 + xd + 3] = img->pixel_data[ys + xs + 3]; // A
			}
			(*d)[y0 + xd + 2] = img->pixel_data[ys + xs];     // R
			(*d)[y0 + xd + 1] = img->pixel_data[ys + xs + 1]; // G
			(*d)[y0 + xd + 0] = img->pixel_data[ys + xs + 2]; // B
		}
	}
}

static void setup_images (MetersLV2UI* ui) {
	struct MyGimpImage const * img;
	switch(ui->type) {
		default:
		case MT_VU:
			img = (struct MyGimpImage const *) &img_vu;
			break;
		case MT_BBC:
			img = (struct MyGimpImage const *) &img_bbc;
			break;
		case MT_EBU:
			img = (struct MyGimpImage const *) &img_ebu;
			break;
		case MT_DIN:
			img = (struct MyGimpImage const *) &img_din;
			break;
		case MT_NOR:
			img = (struct MyGimpImage const *) &img_nor;
			break;
		case MT_COR:
			img = (struct MyGimpImage const *) &img_cor;
			break;
	}
	img2surf(img, &ui->bg, &ui->img0);
	img2surf((struct MyGimpImage const *)&img_screw, &ui->adj, &ui->img1);
}

static void draw_background (MetersLV2UI* ui, cairo_t* cr, float xoff, float yoff) {
	cairo_set_source_surface(cr, ui->bg, xoff, yoff);
	cairo_paint(cr);
}

static void draw_needle (MetersLV2UI* ui, cairo_t* cr, float val,
		const float xoff, const float yoff, const float * const col, const float lw) {
	cairo_save(cr);

	/* needle area */
	cairo_rectangle (cr, xoff, 0, 300, 135);
	cairo_clip (cr);

	/* draw needle */
	const float _xc = 149.5 + xoff;
	const float _yc = 209.5 + yoff;
	const float _r1 = 0;
	const float _r2 = 180;
	float  c, s;

	if (val < 0.00f) val = 0.00f;
	if (val > 1.05f) val = 1.05f;
	val = (val - 0.5f) * 1.5708f;
	c = cosf (val);
	s = sinf (val);
	cairo_new_path (cr);
	cairo_move_to (cr, _xc + s * _r1, _yc - c * _r1);
	cairo_line_to (cr, _xc + s * _r2, _yc - c * _r2);
	cairo_set_source_rgb (cr, col[0], col[1], col[2]);
	cairo_set_line_width (cr, lw);
	cairo_stroke (cr);

	cairo_restore(cr);
}

static gboolean expose_event(GtkWidget *w, GdkEventExpose *event, gpointer handle) {
	MetersLV2UI* ui = (MetersLV2UI*)handle;
	float const * col;
	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));

	switch(ui->type) {
		case MT_VU:
			col = c_blk;
			break;
		default:
			col = c_wht;
			break;
	}

	if (ui->type == MT_COR) {
		draw_background (ui, cr, 0, 0);
		draw_needle (ui, cr, ui->lvl[0], 0, 0, col, 2.0);
		cairo_destroy (cr);
		return TRUE;
	}
	else if (ui->type == MT_BBC && ui->chn == 2) {
		draw_background (ui, cr, 0, 0);
		draw_needle (ui, cr, ui->lvl[0], 0, 0, c_red, 2.0);
		draw_needle (ui, cr, ui->lvl[1], 0, 0, c_grn, 2.0);
	} else {
		int c;
		for (c=0; c < ui->chn; ++c) {
			draw_background (ui, cr, 300.0 * c, 0);
			draw_needle (ui, cr, ui->lvl[c], 300.0 * c, 0, col, 1.4);
		}
	}

	/* draw callibration screw */

	if (ui->drag_x >= 0 || ui->drag_y >=0) {
		int tw, th;
		char buf[48];
		/* default gain -18.0dB in meters.cc, except DIN: -15dB (deflection) */
		switch (ui->type) {
			case MT_VU:
				sprintf(buf, "0 VU = %.1f dBFS", -36 - ui->cal);
				break;
			case MT_BBC:
				sprintf(buf, " '4' = %.1f dBFS", -36 - ui->cal);
				break;
			case MT_DIN:
				/* -3dBu = '-9' ^= -18 dbFS*/
				sprintf(buf, " 0dBu = %.1f dBFS", -30 - ui->cal);
				break;
			case MT_EBU:
			case MT_NOR:
				sprintf(buf, " 'TEST' = %.1f dBFS", -36 - ui->cal);
				break;
			default:
				/* not reached */
				break;
		}

		cairo_save(cr);
		PangoContext * pc = gtk_widget_get_pango_context(w);
		PangoLayout * pl = pango_layout_new (pc);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
		pango_layout_set_text(pl, buf, -1);
		pango_layout_get_pixel_size(pl, &tw, &th);
		cairo_translate (cr, s_xc + s_w2 + 8, s_yc - th/2);
		pango_cairo_layout_path(cr, pl);
		pango_cairo_show_layout(cr, pl);
		g_object_unref(pl);
		cairo_restore(cr);
		cairo_new_path (cr);
	}

	cairo_save(cr);
	cairo_translate (cr, s_xc, s_yc);
	cairo_rotate (cr, ui->cal_rad);
	cairo_translate (cr, -s_w2, -s_h2);
	cairo_set_source_surface (cr, ui->adj, 0, 0);
	cairo_paint(cr);
	cairo_restore(cr);

	cairo_save(cr);
	cairo_translate (cr, s_xc, s_yc);
	cairo_set_source_rgba (cr, 0.20, 0.20, 0.20, 0.8);
	cairo_arc(cr, 0, 0, 12.5, 0, 2 * M_PI);
	cairo_set_line_width (cr, 1.0);
	cairo_stroke(cr);
	cairo_restore(cr);

	cairo_destroy (cr);

	return TRUE;
}

static float cal2rad(float v) {
	/* rotate screw  [-30..0]  -> [-M_PI/4 .. M_PI/4] */
	return .0523583 * (v + 15.0);
}


/* calibration screw drag/drop handling */
static gboolean mousedown(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	MetersLV2UI* ui = (MetersLV2UI*)handle;

	if (   event->x < s_xc - s_w2
			|| event->x > s_xc + s_w2
			|| event->y < s_yc - s_h2
			|| event->y > s_yc + s_h2
			) {
		/* outside of adj-screw area */
		return TRUE;
	}

	if (event->state & GDK_SHIFT_MASK) {
		/* shift-click -> reset to default */
		switch(ui->type) {
			case MT_VU: ui->cal = -22; break;
			case MT_DIN: ui->cal = -15; break;
			default: ui->cal = -18; break;
		}
		ui->write(ui->controller, 0, sizeof(float), 0, (const void*) &ui->cal);
		ui->cal_rad = cal2rad(ui->cal);
		gtk_widget_queue_draw(ui->m0);
		return TRUE;
	}

	ui->drag_x = event->x;
	ui->drag_y = event->y;
	ui->drag_cal = ui->cal;
	return TRUE;
}

static gboolean mouseup(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	MetersLV2UI* ui = (MetersLV2UI*)handle;
	ui->drag_x = ui->drag_y = -1;
	return TRUE;
}

static gboolean mousemove(GtkWidget *w, GdkEventMotion *event, gpointer handle) {
	MetersLV2UI* ui = (MetersLV2UI*)handle;
	if (ui->drag_x < 0 || ui->drag_y < 0) return FALSE;

	const float diff = rint( (event->x - ui->drag_x) - (event->y - ui->drag_y) / 5.0 ) * .5;
	float cal = ui->drag_cal + diff;
	if (cal < -30.0) cal = -30.0;
	if (cal > 0.0) cal = 0.0;

	//printf("Mouse move.. %f %f -> %f   (%f -> %f)\n", event->x, event->y, diff, ui->drag_cal, cal);
  ui->write(ui->controller, 0, sizeof(float), 0, (const void*) &cal);
	ui->cal = cal;
	ui->cal_rad = cal2rad(ui->cal);
	gtk_widget_queue_draw(ui->m0);

	return TRUE;
}

static float meter_deflect(int type, float v) {
	switch(type) {
		case MT_VU:
			return 5.6234149f * v;
		case MT_BBC:
		case MT_EBU:
			v *= 3.17f;
			if (v < 0.1f) return v * 0.855f;
			else return 0.3f * logf (v) + 0.77633f;
		case MT_DIN:
			v = sqrtf (sqrtf (2.002353f * v)) - 0.1885f;
			return (v < 0.0f) ? 0.0f : v;
		case MT_NOR:
			return .4166666f * log10(v) + 1.125f; // (20.0/48.0) *log(v) + (54/48.0)  -> -54dBFS ^= 0, -12dB ^= 1.0
		case MT_COR:
			return 0.5f * (1.0f + v);
		default:
			return 0;
	}
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
	MetersLV2UI* ui = (MetersLV2UI*)malloc(sizeof(MetersLV2UI));
	*widget = NULL;

	ui->type = 0;
	if      (!strcmp(plugin_uri, MTR_URI "VUmono"))    { ui->chn = 1; ui->type = MT_VU; }
	else if (!strcmp(plugin_uri, MTR_URI "VUstereo"))  { ui->chn = 2; ui->type = MT_VU; }
	else if (!strcmp(plugin_uri, MTR_URI "BBCmono"))   { ui->chn = 1; ui->type = MT_BBC; }
	else if (!strcmp(plugin_uri, MTR_URI "BBCstereo")) { ui->chn = 2; ui->type = MT_BBC; }
	else if (!strcmp(plugin_uri, MTR_URI "EBUmono"))   { ui->chn = 1; ui->type = MT_EBU; }
	else if (!strcmp(plugin_uri, MTR_URI "EBUstereo")) { ui->chn = 2; ui->type = MT_EBU; }
	else if (!strcmp(plugin_uri, MTR_URI "DINmono"))   { ui->chn = 1; ui->type = MT_DIN; }
	else if (!strcmp(plugin_uri, MTR_URI "DINstereo")) { ui->chn = 2; ui->type = MT_DIN; }
	else if (!strcmp(plugin_uri, MTR_URI "NORmono"))   { ui->chn = 1; ui->type = MT_NOR; }
	else if (!strcmp(plugin_uri, MTR_URI "NORstereo")) { ui->chn = 2; ui->type = MT_NOR; }
	else if (!strcmp(plugin_uri, MTR_URI "COR"))       { ui->chn = 1; ui->type = MT_COR; }

	if (ui->type == 0) {
		free(ui);
		return NULL;
	}

	ui->write      = write_function;
	ui->controller = controller;
	ui->lvl[0]     = ui->lvl[1] = 0;
	ui->cal        = -18.0;
	ui->cal_rad    = cal2rad(ui->cal);
	ui->box        = gtk_vbox_new(FALSE, 4);
	ui->m0         = gtk_drawing_area_new();
	ui->bg         = NULL;
	ui->adj        = NULL;
	ui->img0       = NULL;
	ui->drag_x     = ui->drag_y = -1;

	setup_images(ui);

	switch (ui->type) {
		case MT_BBC:
			gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), 300, 170);
			break;
		default:
			gtk_drawing_area_size(GTK_DRAWING_AREA(ui->m0), 300 * ui->chn, 170);
			break;
	}

	if (ui->type != MT_COR) {
		gtk_widget_add_events(ui->m0, GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	}

	g_signal_connect (G_OBJECT (ui->m0), "expose_event", G_CALLBACK (expose_event), ui);
	g_signal_connect (G_OBJECT (ui->m0), "button-press-event", G_CALLBACK (mousedown), ui);
	g_signal_connect (G_OBJECT (ui->m0), "button-release-event", G_CALLBACK (mouseup), ui);
	g_signal_connect (G_OBJECT (ui->m0), "motion-notify-event", G_CALLBACK (mousemove), ui);

	gtk_box_pack_start(GTK_BOX(ui->box), ui->m0, FALSE, FALSE, 0);
	gtk_widget_show(ui->m0);

	*widget = ui->box;

	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	MetersLV2UI* ui = (MetersLV2UI*)handle;
	cairo_surface_finish(ui->bg);
	cairo_surface_finish(ui->adj);
	free(ui->img0);
	free(ui->img1);
	free(ui);
}


static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	MetersLV2UI* ui = (MetersLV2UI*)handle;

  if ( format != 0 ) { return; }

	if (port_index == 3) {
		ui->lvl[0] = meter_deflect(ui->type, *(float *)buffer);
		gtk_widget_queue_draw(ui->m0);
	} else
	if (port_index == 6) {
		ui->lvl[1] = meter_deflect(ui->type, *(float *)buffer);
		gtk_widget_queue_draw(ui->m0);
	} else
	if (port_index == 0) {
		ui->cal = *(float *)buffer;
		ui->cal_rad = cal2rad(ui->cal);
		gtk_widget_queue_draw(ui->m0);
	}

#if 0 // TODO invalidate changed area only.
		GdkRectangle update_rect;
		update_rect.x = 0
		update_rect.y = 0
		update_rect.width = 10;
		update_rect.height = 10;
		gtk_widget_queue_draw_area(ui->m0, update_rect.x, update_rect.y, update_rect.width, update_rect.height);
#endif
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
	MTR_URI "ui",
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
