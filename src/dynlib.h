/*
 * dynlib.h - generic support for dynamic library loading.
 *
 * Written by
 *  Christian Vogelgsang <chris@vogelgsang.org>
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

#ifndef VICE_DYNLIB_H
#define VICE_DYNLIB_H

#include "vice.h"
#include "types.h"

/* This hack/function will only ever be needed on windows, so
   make an empty define here (instead of stub functions) */
#if defined(WINDOWS_COMPILE)
void archdep_opencbm_fix_dll_path(void);
#else
#define archdep_opencbm_fix_dll_path()
#endif

void *vice_dynlib_open(const char *name);
void *vice_dynlib_symbol(void *handle, const char *name);
char *vice_dynlib_error(void);
int vice_dynlib_close(void *handle);

#endif
