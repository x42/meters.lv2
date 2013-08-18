#include <gtk/gtk.h>
#include <cairo/cairo.h>

typedef struct {
	GtkWidget* w;
	GtkWidget* c;

	float min;
	float max;
	float acc;
	float cur;

	float drag_x, drag_y, drag_c;

	gboolean (*cb) (GtkWidget* w, gpointer handle);
	gpointer handle;

} GtkExtDial;

#define GED_BOUNDS 25
#define GED_RADIUS 10
#define GED_CX 12.5
#define GED_CY 12.5

static gboolean gtkext_dial_expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	GtkStyle *style = gtk_widget_get_style(w);
	GdkColor *c = &style->bg[GTK_STATE_NORMAL];
	cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	cairo_rectangle (cr, 0, 0, GED_BOUNDS, GED_BOUNDS);
	cairo_fill(cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	cairo_set_source_rgba (cr, .35, .36, .37, 1.0);
	//cairo_move_to(cr, GED_CX, GED_CY);
	cairo_arc (cr, GED_CX, GED_CY, GED_RADIUS, 0, 2.0 * M_PI);
	cairo_fill_preserve (cr);
	cairo_set_line_width(cr, .75);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_stroke (cr);

	cairo_set_source_rgba (cr, .95, .95, .95, 1.0);
	cairo_set_line_width(cr, 1.5);
	cairo_move_to(cr, GED_CX, GED_CY);
	float ang = (.75 * M_PI) + (1.5 * M_PI) * (d->cur - d->min) / d->max;
	float wid = M_PI * 2 / 180.0;
	cairo_arc (cr, GED_CX, GED_CY, GED_RADIUS, ang-wid, ang+wid);
	cairo_stroke (cr);

	cairo_destroy (cr);
	return TRUE;
}

static gboolean gtkext_dial_mousedown(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	d->drag_x = event->x;
	d->drag_y = event->y;
	d->drag_c = d->cur;
	gtk_widget_queue_draw(d->w);
	return TRUE;
}

static gboolean gtkext_dial_mouseup(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	d->drag_x = d->drag_y = -1;
	gtk_widget_queue_draw(d->w);
	return TRUE;
}

static gboolean gtkext_dial_mousemove(GtkWidget *w, GdkEventMotion *event, gpointer handle) {
	GtkExtDial * d = (GtkExtDial *)handle;
	if (d->drag_x < 0 || d->drag_y < 0) return FALSE;
	float diff = ((event->x - d->drag_x) - (event->y - d->drag_y)) * 0.004; // 250px full-scale
	diff = rint(diff * (d->max - d->min) / d->acc ) * d->acc;
	float val = d->drag_c + diff;
	if (val < d->min) val = d->min;
	if (val > d->max) val = d->max;
	if (val != d->cur) {
		d->cur = val;
		if (d->cb) d->cb(d->w, d->handle);
		gtk_widget_queue_draw(d->w);
	}
	return TRUE;
}

/******************************************************************************
 * public functions
 */

GtkExtDial * gtkext_dial_new(float min, float max, float step) {
	assert(max > min);
	assert( (max - min) / step <= 250.0);
	assert( (max - min) / step >= 1.0);

	GtkExtDial *d = malloc(sizeof(GtkExtDial));
	d->w = gtk_drawing_area_new();
	d->c = gtk_alignment_new(.5, .5, 0, 0);
	gtk_container_add(GTK_CONTAINER(d->c), d->w);

	d->cb = NULL;
	d->handle = NULL;
	d->min = min;
	d->max = max;
	d->acc = step;
	d->cur = min;

	gtk_drawing_area_size(GTK_DRAWING_AREA(d->w), GED_BOUNDS, GED_BOUNDS);
	gtk_widget_set_size_request(d->w, GED_BOUNDS, GED_BOUNDS);

	gtk_widget_set_redraw_on_allocate(d->w, TRUE);
	gtk_widget_add_events(d->w, GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	g_signal_connect (G_OBJECT (d->w), "expose_event", G_CALLBACK (gtkext_dial_expose_event), d);
	g_signal_connect (G_OBJECT (d->w), "button-release-event", G_CALLBACK (gtkext_dial_mouseup), d);
	g_signal_connect (G_OBJECT (d->w), "button-press-event",   G_CALLBACK (gtkext_dial_mousedown), d);
	g_signal_connect (G_OBJECT (d->w), "motion-notify-event",  G_CALLBACK (gtkext_dial_mousemove), d);

	return d;
}

GtkWidget * gtkext_dial_wiget(GtkExtDial *d) {
	return d->c;
}

void gtkext_dial_set_callback(GtkExtDial *d, gboolean (*cb) (GtkWidget* w, gpointer handle), gpointer handle) {
	d->cb = cb;
	d->handle = handle;
}

void gtkext_dial_destroy(GtkExtDial *d) {
	gtk_widget_destroy(d->w);
	gtk_widget_destroy(d->c);
	free(d);
}

void gtkext_dial_set_value(GtkExtDial *d, float v) {
	v = d->min + rint((v-d->min) / d->acc ) * d->acc;
	if (v < d->min) d->cur = d->min;
	else if (v > d->max) d->cur = d->max;

	if (v != d->cur) {
		d->cur = v;
		if (d->cb) d->cb(d->w, d->handle);
		gtk_widget_queue_draw(d->w);
	}

	gtk_widget_queue_draw(d->w);
}

float gtkext_dial_get_value(GtkExtDial *d) {
	return (d->cur);
}
