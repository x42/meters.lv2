/* goniometer LV2 GUI
 *
 * Copyright 2013 Robin Gareus <robin@gareus.org>
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

#include <string.h>
#include <stdlib.h>

/* simple lockless ringbuffer
 * for goniometer stereo signal
 */

typedef struct {
	float *c0;
	float *c1;

	size_t rp;
	size_t wp;
	size_t len;
} jfringbuf;

jfringbuf * jfrb_alloc(size_t siz) {
	jfringbuf *rb  = (jfringbuf*) malloc(sizeof(jfringbuf));
	rb->c0 = (float*) malloc(siz * sizeof(float));
	rb->c1 = (float*) malloc(siz * sizeof(float));
	rb->len = siz;
	rb->rp = 0;
	rb->wp = 0;
	return rb;
}

void jfrb_free(jfringbuf *rb) {
	free(rb->c0);
	free(rb->c1);
	free(rb);
}

size_t jfrb_write_space(jfringbuf *rb) {
	if (rb->rp == rb->wp) return (rb->len -1);
	return ((rb->len + rb->rp - rb->wp) % rb->len) -1;
}

size_t jfrb_read_space(jfringbuf *rb) {
	return ((rb->len + rb->wp - rb->rp) % rb->len);
}

int jfrb_read_one(jfringbuf *rb, float *c0, float *c1) {
	if (jfrb_read_space(rb) < 1) return -1;
	*c0 = rb->c0[rb->rp];
	*c1 = rb->c1[rb->rp];
	rb->rp = (rb->rp + 1) % rb->len;
	return 0;
}

int jfrb_write(jfringbuf *rb, float *c0, float *c1, size_t len) {
	if (jfrb_write_space(rb) < len) return -1;
	if (rb->wp + len <= rb->len) {
		memcpy((void*) &rb->c0[rb->wp], (void*) c0, len * sizeof(float));
		memcpy((void*) &rb->c1[rb->wp], (void*) c1, len * sizeof(float));
	} else {
		int part = rb->len - rb->wp;
		int remn = len - part;

		memcpy((void*) &rb->c0[rb->wp], (void*) c0, part * sizeof(float));
		memcpy((void*) &rb->c1[rb->wp], (void*) c1, part * sizeof(float));

		memcpy((void*) rb->c0, (void*) &c0[part], remn * sizeof(float));
		memcpy((void*) rb->c1, (void*) &c1[part], remn * sizeof(float));
	}
	rb->wp = (rb->wp + len) % rb->len;
	return 0;
}

void jfrb_read_clear(jfringbuf *rb) {
	rb->rp = rb->wp;
}


/* goniometer shared instance struct */

typedef struct {
	/* private */
	float* input[2];
	float* output[2];
	float* notify;

	double rate;
	bool ui_active;

	uint32_t ntfy;
	uint32_t apv;
	uint32_t sample_cnt;

	/* shared with ui */
	jfringbuf *rb;

} LV2jf;