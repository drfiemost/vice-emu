/*
 * fixpoint.c - Fixed point routines.
 *
 * Written by
 *  Andreas Dehmel <dehmel@forwiss.tu-muenchen.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef _FIXPOINT_C
#define _FIXPOINT_C

#ifdef _FIXPOINT_H
#define FIXPOINT_FROM_HEADER
#endif

#include "fixpoint.h"

#ifdef FIXPOINT_ARITHMETIC

#ifdef INLINE_FIXPOINT_FUNCTIONS
#define FUNC_INLINE_STATEMENTstatic inline
#else
#define FUNC_INLINE_STATEMENT
#endif

/* Only make the function(s) visible if no inlining or inlining _and_ we're
 *  not compiling fixpoint.c */
#if (!defined(INLINE_FIXPOINT_FUNCTIONS) || (defined(INLINE_FIXPOINT_FUNCTIONS) && defined(FIXPOINT_FROM_HEADER)))

FUNC_INLINE_STATEMENT vreal_t fixpoint_mult(vreal_t x, vreal_t y)
{
    unsigned int a, b, c, d;
    int sign;

    sign = ((x ^ y) < 0) ? 1 : 0;
    if (x < 0)
	x = -x;
    if (y < 0)
	y = -y;
    a = (((unsigned int) x) >> FIXPOINT_PREC);
    b = ((unsigned int) x) & ~(a << FIXPOINT_PREC);
    c = (((unsigned int) y) >> FIXPOINT_PREC);
    d = ((unsigned int) y) & ~(c << FIXPOINT_PREC);
    a = (((a * c) << FIXPOINT_PREC)
         + (a * d + b * c)
         + (((b * d) + (1 << (FIXPOINT_PREC - 1))) >> FIXPOINT_PREC));
    if (sign != 0)
	a = -a;
    return a;
}

#endif

#endif

#endif
