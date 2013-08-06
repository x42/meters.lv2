#include <stdio.h>
#include <math.h>
#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

/* colors */
static const float c_red[4] = {1.0, 0.0, 0.0, 1.0};
static const float c_grn[4] = {0.0, 1.0, 0.0, 1.0};
static const float c_blk[4] = {0.0, 0.0, 0.0, 1.0};
static const float c_wht[4] = {1.0, 1.0, 1.0, 1.0};
static const float c_wsh[4] = {1.0, 1.0, 1.0, 0.5};

static const float rl = 180;
static const float rs = 170;

/* needle end */
static const float _xc = 149.5;
static const float _yc = 209.5;

static void draw_needle (cairo_t* cr, float val, const float _r2,
		const float * const col, const float lw) {
	cairo_save(cr);

	/* needle area */
	cairo_rectangle (cr, 0, 0, 300, 135);
	cairo_clip (cr);

	const float _r1 = 0;
	float  c, s;

	if (val < 0.00f) val = 0.00f;
	if (val > 1.05f) val = 1.05f;
	val = (val - 0.5f) * 1.5708f;
	c = cosf (val);
	s = sinf (val);
	cairo_new_path (cr);
	cairo_move_to (cr, _xc + s * _r1, _yc - c * _r1);
	cairo_line_to (cr, _xc + s * _r2, _yc - c * _r2);
	cairo_set_source_rgba (cr, col[0], col[1], col[2], col[3]);
	cairo_set_line_width (cr, lw);
	cairo_stroke (cr);

	cairo_restore(cr);
}

float radi (float v) {
	return (-M_PI/2 - M_PI/4) + v * M_PI/2;
}

float deflect_nordic(float db) {
	return (1.0/48.0) * (db-18) + (54/48.0);
}

void clear_center(cairo_t* cr, float r) {
	cairo_set_source_rgba (cr, .0, .0, .0, .0);
	cairo_arc (cr, 149.5, 209.5, r, 0, 2 * M_PI);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_fill (cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

}

void write_text(cairo_t* cr, char *txt, char *font, float x, float y, float ang) {
	int tw, th;
	cairo_save(cr);
	PangoLayout * pl = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(pl, desc);
	pango_font_description_free(desc);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, &tw, &th);
	cairo_translate (cr, x, y);
	cairo_rotate (cr, ang);
	cairo_translate (cr, -tw/2.0 - 0.5, -th/2.0);
	pango_cairo_layout_path(cr, pl);
	pango_cairo_show_layout(cr, pl);
	g_object_unref(pl);
	cairo_restore(cr);
	cairo_new_path (cr);
}

void needle_label(cairo_t* cr, char *txt, float val, const float _r2) {
	float  c, s;
	if (val < 0.00f) val = 0.00f;
	if (val > 1.05f) val = 1.05f;
	val = (val - 0.5f) * 1.5708f;
	c = cosf (val);
	s = sinf (val);

	write_text(cr, txt, "Sans Bold 9", _xc + s * _r2, _yc - c * _r2,  val);
}


void nordic_triangle(cairo_t* cr, float val, const float _r2) {
	float  c, s;
	if (val < 0.00f) val = 0.00f;
	if (val > 1.05f) val = 1.05f;
	val = (val - 0.5f) * 1.5708f;
	c = cosf (val);
	s = sinf (val);

	cairo_save(cr);
	cairo_translate (cr, _xc + s * _r2, _yc - c * _r2);
	cairo_rotate (cr, val);
	cairo_move_to (cr,  0.0, 10.0);
	cairo_line_to (cr,  6.9282,  -2);
	cairo_line_to (cr, -6.9282,  -2);
	cairo_line_to (cr,  0,  10.0);
	cairo_set_line_width (cr, 1.2);
	cairo_set_source_rgba (cr, .9, .2, .2, 1.0); // nordic red
	cairo_fill_preserve (cr);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	cairo_stroke (cr);

	cairo_restore(cr);
}



void nordic(cairo_t* cr) {
#if 0
	cairo_set_source_rgba (cr, .9, .2, .2, 1.0);
	cairo_arc (cr, 149.5, 209.5, 160, -M_PI/2 - M_PI/4, -M_PI/2 + M_PI/4 );
	cairo_set_line_width (cr, 9);
	cairo_stroke (cr);
#endif

	int db; char buf[48];
	for (db = -36; db <= 12 ; db+=6) {
		float v = deflect_nordic(db);
		if (db == 0) {
			nordic_triangle(cr, v, rs);
			needle_label(cr, "TEST\n", v, rl);
			continue;
		}
		draw_needle(cr, v,  rl, c_wht, 1.5);
		if (db == 12) {
			continue;
		}
		sprintf(buf, "%+d\n", db);
		needle_label(cr, buf, v, rl);
	}
	for (db = -33; db <= 12 ; db+=6) {
		float v = deflect_nordic(db);
		draw_needle(cr, v,  rs, c_wht, 1.5);
		if (db == 9) {
			sprintf(buf, "%+d", db);
			needle_label(cr, buf, v, rl);
		}
	}

	clear_center(cr, 160);

	cairo_arc (cr, 149.5, 209.5, 155,
			radi(deflect_nordic(6)),
			radi(deflect_nordic(12)));

	cairo_set_line_width (cr, 12.5);
	cairo_set_source_rgba (cr, 1,1,1,1);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgba (cr, .9, .2, .2, 1.0); // nordic red
	cairo_set_line_width (cr, 10);
	cairo_stroke(cr);

	draw_needle(cr, deflect_nordic(6),  160, c_wht, 1.5);
	draw_needle(cr, deflect_nordic(12),  160, c_wht, 1.5);
	clear_center(cr, 149);
	//write_text(cr, "dB", "Sans Bold 10", 150, 87, 0);
	write_text(cr, "dB", "Sans Bold 12", 150, 90, 0);

}


void phase(cairo_t* cr) {

	cairo_set_source_rgba (cr, .9, .2, .2, 1.0);
	cairo_arc (cr, 149.5, 209.5, 160, -M_PI/2 - M_PI/4, -M_PI/2 );
	cairo_set_line_width (cr, 20);
	cairo_stroke (cr);

	cairo_set_source_rgba (cr, .2, .9, .2, 1.0);
	cairo_arc (cr, 149.5, 209.5, 160, -M_PI/2, -M_PI/2 + M_PI/4 );
	cairo_set_line_width (cr, 20);
	cairo_stroke (cr);

#if 1 // S*fam
	cairo_arc (cr, 149.5, 209.5, 170, -M_PI/2 - M_PI/8, -M_PI/2 + M_PI/8 );
	cairo_arc_negative (cr, 200, 129.5, 80, -M_PI/2 + M_PI/36 , -M_PI/2 - M_PI/8);
	cairo_arc_negative(cr, 149.5, 209.5, 160, -M_PI/2,  -M_PI/2 - M_PI/36);
	cairo_arc_negative(cr, 99, 129.5, 80.5, -M_PI/2 + M_PI/8  , -M_PI/2 - M_PI/36);
	cairo_set_source_rgba (cr, .9, .9, .0, 1.0);
#if 0
	cairo_fill(cr);
#else
	cairo_fill_preserve (cr);
	cairo_set_source_rgba (cr, 1, 1, 1, 1);
	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);
#endif

#endif

	cairo_set_source_rgba (cr, 1, 1, 1, 1);
	cairo_arc (cr, 149.5, 209.5, 160, -M_PI/2 - M_PI/4, -M_PI/2 + M_PI/4 );
	cairo_set_line_width (cr, 2.5);
	cairo_stroke (cr);

	int v;
	for (v = 0; v <= 20 ; v++) {
		if (v == 0 || v == 5 || v == 10 || v == 15 || v == 20) {
			continue;
			draw_needle(cr, v/20.0,  rl, c_wht, 2.0);
		} else {
			draw_needle(cr, v/20.0,  rs, c_wsh, 1.5);
		}
	}
	clear_center(cr, 160);

	draw_needle(cr, 0.0,  rl-5, c_wht, 2.0);
	draw_needle(cr, 0.25, rl-5, c_wht, 2.0);
	draw_needle(cr, 0.5,  rl-5, c_wht, 2.0);
	draw_needle(cr, 0.75, rl-5, c_wht, 2.0);
	draw_needle(cr, 1.0,  rl-5, c_wht, 2.0);

	clear_center(cr, 155);

	needle_label(cr, "-1",     0, rs-24);
	needle_label(cr, "-0.5", .25, rs-24);
	needle_label(cr, "0",    0.5, rs-24);
	needle_label(cr, "+0.5", .75, rs-24);
	needle_label(cr, "+1",   1.0, rs-24);

	needle_label(cr, " 180\u00B0",   0, rl+2);
	needle_label(cr, " 135\u00B0", .25, rl+2);
	needle_label(cr,  " 90\u00B0", 0.5, rl+2);
	needle_label(cr,  " 45\u00B0", .75, rl+2);
	needle_label(cr,   " 0\u00B0", 1.0, rl+2);

}


int main(int argc, char **argv) {
	char *filename = "/tmp/image1.png";
	if (argc > 1) filename = argv[1];

	cairo_surface_t * cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 300, 170);
	cairo_t* cr = cairo_create(cs);

	//nordic(cr);
	phase(cr);

	cairo_surface_write_to_png(cs, filename);
	return 0;
}
