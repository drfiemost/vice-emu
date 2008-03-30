/*
 * uiramcart.c
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

#include "uiramcart.h"
#include "intl.h"
#include "translate.h"

static const char *ui_ramcart_enable_translate[] = {
  IDMS_DISABLED,
  IDS_ENABLED,
  0
};

static char *ui_ramcart_enable[countof(ui_ramcart_enable_translate)];

static const int ui_ramcart_enable_values[] = {
  0,
  1,
  -1
};

static int ui_ramcart_read_only_translate[] = {
  IDS_READ_WRITE,
  IDS_READ_ONLY,
  0
};

static char *ui_ramcart_read_only[countof(ui_ramcart_read_only_translate)];

static const int ui_ramcart_read_only_values[] = {
  0,
  1,
  -1
};

static const char *ui_ramcart_size[] = {
  "64K",
  "128K",
  NULL
};

static const int ui_ramcart_size_values[] = {
  64,
  128,
  -1
};

static ui_to_from_t ui_to_from[] = {
  { NULL, MUI_TYPE_CYCLE, "RAMCART", ui_ramcart_enable, ui_ramcart_enable_values },
  { NULL, MUI_TYPE_CYCLE, "RAMCART_RO", ui_ramcart_read_only, ui_ramcart_read_only_values },
  { NULL, MUI_TYPE_CYCLE, "RAMCARTsize", ui_ramcart_size, ui_ramcart_size_values },
  UI_END /* mandatory */
};

static APTR build_gui(void)
{
  return GroupObject,
    CYCLE(ui_to_from[0].object, "RAMCART", ui_ramcart_enable)
    CYCLE(ui_to_from[1].object, translate_text(IDS_RAMCART_READ_WRITE), ui_ramcart_read_only)
    CYCLE(ui_to_from[2].object, translate_text(IDS_RAMCART_SIZE), ui_ramcart_size)
  End;
}

void ui_ramcart_settings_dialog(void)
{
  intl_convert_mui_table(ui_ramcart_enable_translate, ui_ramcart_enable);
  intl_convert_mui_table(ui_ramcart_read_only_translate, ui_ramcart_read_only);
  mui_show_dialog(build_gui(), translate_text(IDS_RAMCART_SETTINGS), ui_to_from);
}
