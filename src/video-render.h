/*
 * video-render.h - Video implementation for Win32, using DirectDraw.
 *
 * Written by
 *  John Selck <graham@cruise.de>
 *  Andreas Boose <boose@linux.rz.fh-hannover.de>
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

#ifndef _VIDEORENDER_H
#define _VIDEORENDER_H

#include "types.h"

extern void video_render_main(DWORD *colortab, BYTE *src, BYTE *trg, int width,
                              int height, int xs, int ys, int xt, int yt,
                              int pitchs, int pitcht, int depth);

#endif

