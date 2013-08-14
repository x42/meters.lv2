/* Copyright (C) 2013 Robin Gareus <robin@gareus.org>
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


#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "truepeakdsp.h"

namespace LV2M {

TruePeakdsp::TruePeakdsp (void)
	: _m (0)
	, _res (true)
	, _buf (NULL)
{
}


TruePeakdsp::~TruePeakdsp (void)
{
	free(_buf);
}


void TruePeakdsp::process (float *p, int n)
{
	assert (n <= 8192);
	_src.inp_count = n;
	_src.inp_data = p;
	_src.out_count = n * 4;
	_src.out_data = _buf;
	_src.process ();

	float m = _res ? 0 : _m;
	float v;
	float *b = _buf;
	while (n--) {
		v = fabsf(*b++);
		if (v > m) m = v;
		v = fabsf(*b++);
		if (v > m) m = v;
		v = fabsf(*b++);
		if (v > m) m = v;
		v = fabsf(*b++);
		if (v > m) m = v;
	}
	_m = m;
}


float TruePeakdsp::read (void)
{
    _res = true;
    return _m;
}


void TruePeakdsp::init (float fsamp)
{
	_src.setup(fsamp, fsamp * 4.0, 1, 32);
	_buf = (float*) malloc(32768 * sizeof(float));

	/* q/d initialize */
	float zero[8192];
	for (int i = 0; i < 8192; ++i) {
		zero[i]= 0.0;
	}
	_src.inp_count = 8192;
	_src.inp_data = zero;
	_src.out_count = 32768;
	_src.out_data = _buf;
	_src.process ();
}

};
