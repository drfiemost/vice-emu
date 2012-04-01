/*
 * mousedrv.h - Mouse handling for Win32.
 *
 * Written by
 *  Tibor Biczo <crown@mail.matav.hu>
 *  Ettore Perazzoli <ettore@comm2000.it>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef VICE_MOUSEDRV_H
#define VICE_MOUSEDRV_H

#include "types.h"

extern int mousedrv_resources_init(void);
extern int mousedrv_cmdline_options_init(void);
extern void mousedrv_init(void);

extern void mousedrv_mouse_changed(void);

extern int mousedrv_get_x(void);
extern int mousedrv_get_y(void);
extern unsigned long mousedrv_get_timestamp(void);

extern void mouse_set_format(void);
extern void mouse_update_mouse(void);
extern void mouse_update_mouse_acquire(void);
extern void mouse_set_cooperative_level(void);

extern int _mouse_x, _mouse_y;
extern unsigned long _mouse_timestamp;

#endif
