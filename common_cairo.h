#ifndef COMMON_CAIRO_H
#define COMMON_CAIRO_H

#include <string.h>
#include <cairo/cairo.h>
#include <pango/pango.h>

static void rounded_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
  double degrees = M_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);
  cairo_arc (cr, x + r, y + h - r, r, 90 * degrees, 180 * degrees);
  cairo_arc (cr, x + r, y + r, r, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);
}

/* colors */
static const float c_blk[4] = {0.0, 0.0, 0.0, 1.0};
static const float c_wht[4] = {1.0, 1.0, 1.0, 1.0};
static const float c_gry[4] = {0.5, 0.5, 0.5, 1.0};
static const float c_lgt[4] = {.9, .9, .9, 1.0};
static const float c_lgg[4] = {.9, .95, .9, 1.0};
static const float c_grb[4] = {.5, .5, .6, 1.0};

static void get_text_geometry( const char *txt, PangoFontDescription *font, int *tw, int *th) {
	cairo_surface_t* tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 8);
	cairo_t *cr = cairo_create (tmp);
	PangoLayout * pl = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(pl, font);
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, tw, th);
	g_object_unref(pl);
	cairo_destroy (cr);
	cairo_surface_destroy(tmp);
}

static void write_text_full(
		cairo_t* cr,
		const char *txt,
		PangoFontDescription *font,
		const float x, const float y,
		const float ang, const int align,
		const float * const col) {
	int tw, th;
	cairo_save(cr);

	PangoLayout * pl = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(pl, font);
	cairo_set_source_rgba (cr, col[0], col[1], col[2], col[3]);
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, &tw, &th);
	cairo_translate (cr, x, y);
	if (ang != 0) { cairo_rotate (cr, ang); }
	switch(abs(align)) {
		case 1:
			cairo_translate (cr, -tw, -th/2.0);
			break;
		case 2:
			cairo_translate (cr, -tw/2.0 - 0.5, -th/2.0);
			break;
		case 3:
			cairo_translate (cr, -0.5, -th/2.0);
			break;
		case 4:
			cairo_translate (cr, -tw, -th);
			break;
		case 5:
			cairo_translate (cr, -tw/2.0 - 0.5, -th);
			break;
		case 6:
			cairo_translate (cr, -0.5, -th);
			break;
		case 7:
			cairo_translate (cr, -tw, 0);
			break;
		case 8:
			cairo_translate (cr, -tw/2.0 - 0.5, 0);
			break;
		case 9:
			cairo_translate (cr, -0.5, 0);
			break;
		default:
			break;
	}
	pango_cairo_layout_path(cr, pl);
	pango_cairo_show_layout(cr, pl);
	g_object_unref(pl);
	cairo_restore(cr);
	cairo_new_path (cr);
}

void get_color_from_gtk (GdkColor *c, int which) {
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *foobar = gtk_label_new("Foobar");
	gtk_container_add(GTK_CONTAINER(window), foobar);
	gtk_widget_ensure_style(foobar);

	GtkStyle *style = gtk_widget_get_style(foobar);
	switch (which) {
		default:
			memcpy((void*) c, (void*) &style->fg[GTK_STATE_NORMAL], sizeof(GdkColor));
			break;
		case 1:
			memcpy((void*) c, (void*) &style->bg[GTK_STATE_NORMAL], sizeof(GdkColor));
			break;
	}
	gtk_widget_destroy(foobar);
	gtk_widget_destroy(window);
}

PangoFontDescription * get_font_from_gtk () {
  PangoFontDescription * rv;
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *foobar = gtk_label_new("Foobar");
	gtk_container_add(GTK_CONTAINER(window), foobar);
	gtk_widget_ensure_style(foobar);

	PangoContext* pc = gtk_widget_get_pango_context(foobar);
	PangoFontDescription const * pfd = pango_context_get_font_description (pc);
	rv = pango_font_description_copy (pfd);
	gtk_widget_destroy(foobar);
	gtk_widget_destroy(window);
	return rv;
}

void set_cairo_color_from_gtk (cairo_t *cr, int which) {
	GdkColor color;
	get_color_from_gtk(&color, which);
	cairo_set_source_rgba(cr, color.red/65536.0, color.green/65536.0, color.blue/65536.0, 1.0);
}

void get_cairo_color_from_gtk (int which, float *col) {
	GdkColor color;
	get_color_from_gtk(&color, which);
	col[0] = color.red/65536.0;
	col[1] = color.green/65536.0;
	col[2] = color.blue/65536.0;
	col[3] = 1.0;
}


#endif
