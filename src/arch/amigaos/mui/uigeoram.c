/*
 * uigeoram.c
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
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

#include "vice.h"
#ifdef AMIGA_M68K
#define _INLINE_MUIMASTER_H
#endif
#include "mui.h"

#include "uigeoram.h"

static const char *ui_georam_on_off[] = {
  "off",
  "on",
  NULL
};

static const int ui_georam_on_off_values[] = {
  0,
  1,
  -1
};

static const char *ui_georam_size[] = {
  "64K",
  "128K",
  "256K",
  "512K",
  "1024K",
  "2048K",
  "4096K",
  NULL
};

static const int ui_georam_size_values[] = {
  64,
  128,
  256,
  512,
  1024,
  2048,
  4096,
  -1
};

static ui_to_from_t ui_to_from[] = {
  { NULL, MUI_TYPE_CYCLE, "GEORAM", ui_georam_on_off, ui_georam_on_off_values },
  { NULL, MUI_TYPE_CYCLE, "GEORAMsize", ui_georam_size, ui_georam_size_values },
  UI_END /* mandatory */
};

static APTR build_gui(void)
{
  return GroupObject,
    CYCLE(ui_to_from[0].object, "GEORAM Enabled", ui_georam_on_off)
    CYCLE(ui_to_from[1].object, "GEORAM Size", ui_georam_size)
  End;
}

void ui_georam_settings_dialog(void)
{
  mui_show_dialog(build_gui(), "GEORAM Settings", ui_to_from);
}
