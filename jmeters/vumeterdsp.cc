// ------------------------------------------------------------------------
//
//  Copyright (C) 2012 Fons Adriaensen <fons@linuxaudio.org>
//    
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// ------------------------------------------------------------------------


#include <math.h>
#include "vumeterdsp.h"

namespace LV2M {

float  Vumeterdsp::_w = 0;
float  Vumeterdsp::_g = 0; 


Vumeterdsp::Vumeterdsp (void) :
    _z1 (0),
    _z2 (0),
    _m (0),
    _res (true)
{
}


Vumeterdsp::~Vumeterdsp (void)
{
}


void Vumeterdsp::process (float *p, int n)
{
    float z1, z2, m, t1, t2;

    z1 = _z1 > 20 ? 20 : (_z1 < -20 ? -20 : _z1);
    z2 = _z2 > 20 ? 20 : (_z2 < -20 ? -20 : _z2);
    m = _res ? 0: _m;
    _res = false;

    n /= 4;
    while (n--)
    {
	t2 = z2 / 2;
	t1 = fabsf (*p++) - t2;
	z1 += _w * (t1 - z1);
	t1 = fabsf (*p++) - t2;
	z1 += _w * (t1 - z1);
	t1 = fabsf (*p++) - t2;
	z1 += _w * (t1 - z1);
	t1 = fabsf (*p++) - t2;
	z1 += _w * (t1 - z1);
	z2 += 4 * _w * (z1 - z2);
	if (z2 > m) m = z2;
    }

    if (!isfinite(z1)) {_z1 = 0; m = INFINITY;} else _z1 = z1;
    if (!isfinite(z2)) {_z2 = 0; m = INFINITY;} else _z2 = z2 + 1e-10f;
    _m = m;
}


float Vumeterdsp::read (void)
{
    _res = true;
    return _g * _m;
}


void Vumeterdsp::init (float fsamp)
{
    _w = 11.1f / fsamp; 
    _g = 1.5f * 1.571f;
}

};
/* vi:set ts=8 sts=8 sw=4: */
