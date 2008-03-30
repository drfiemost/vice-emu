/*
 * uidrivec64vic20.c
 *
 * Written by
 *  Mathias Roslund <vice.emu@amidog.se>
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

#include "drive.h"
#include "uidrivec64vic20.h"

static const char *drive_number_strings[] = {
  "Drive 8",
  "Drive 9",
  "Drive 10",
  "Drive 11",
  NULL
};

static const char *drive_type_strings_8[] = {
  "None",
  "1541",
  "1541-II",
  "1570",
  "1571",
  "1581",
  "2031",
  "2040",
  "3040",
  "4040",
  "1001",
  "8050",
  "8250",
  NULL
};

static const int drive_type_values_8[] = {
  DRIVE_TYPE_NONE,
  DRIVE_TYPE_1541,
  DRIVE_TYPE_1541II,
  DRIVE_TYPE_1570,
  DRIVE_TYPE_1571,
  DRIVE_TYPE_1581,
  DRIVE_TYPE_2031,
  DRIVE_TYPE_2040,
  DRIVE_TYPE_3040,
  DRIVE_TYPE_4040,
  DRIVE_TYPE_1001,
  DRIVE_TYPE_8050,
  DRIVE_TYPE_8250,
  -1
};

static const char *drive_type_strings_9[] = {
  "None",
  "1541",
  "1541-II",
  "1570",
  "1571",
  "1581",
  "2031",
  "1001",
  NULL
};

static const int drive_type_values_9[] = {
  DRIVE_TYPE_NONE,
  DRIVE_TYPE_1541,
  DRIVE_TYPE_1541II,
  DRIVE_TYPE_1570,
  DRIVE_TYPE_1571,
  DRIVE_TYPE_1581,
  DRIVE_TYPE_2031,
  DRIVE_TYPE_1001,
  -1
};

static const char *drive_type_strings_10[] = {
  "None",
  "1541",
  "1541-II",
  "1570",
  "1571",
  "1581",
  "2031",
  "1001",
  NULL
};

static const int drive_type_values_10[] = {
  DRIVE_TYPE_NONE,
  DRIVE_TYPE_1541,
  DRIVE_TYPE_1541II,
  DRIVE_TYPE_1570,
  DRIVE_TYPE_1571,
  DRIVE_TYPE_1581,
  DRIVE_TYPE_2031,
  DRIVE_TYPE_1001,
  -1
};

static const char *drive_type_strings_11[] = {
  "None",
  "1541",
  "1541-II",
  "1570",
  "1571",
  "1581",
  "2031",
  "1001",
  NULL
};

static const int drive_type_values_11[] = {
  DRIVE_TYPE_NONE,
  DRIVE_TYPE_1541,
  DRIVE_TYPE_1541II,
  DRIVE_TYPE_1570,
  DRIVE_TYPE_1571,
  DRIVE_TYPE_1581,
  DRIVE_TYPE_2031,
  DRIVE_TYPE_1001,
  -1
};

static const char *drive_extend_strings[] = {
  "Never extend",
  "Ask on extend",
  "Extend on access",
  NULL
};

static const int drive_extend_values[] = {
  DRIVE_EXTEND_NEVER,
  DRIVE_EXTEND_ASK,
  DRIVE_EXTEND_ACCESS,
  -1
};

static const char *drive_idle_strings[] = {
  "No traps",
  "Skip cycles",
  "Trap idle",
  NULL
};

static const int drive_idle_values[] = {
  DRIVE_IDLE_NO_IDLE,
  DRIVE_IDLE_SKIP_CYCLES,
  DRIVE_IDLE_TRAP_IDLE,
  -1
};

#define DECL(device) \
  { NULL, MUI_TYPE_RADIO, "Drive" #device "Type", drive_type_strings_ ## device, drive_type_values_ ## device }, \
  { NULL, MUI_TYPE_RADIO, "Drive" #device "ExtendImagePolicy", drive_extend_strings, drive_extend_values }, \
  { NULL, MUI_TYPE_RADIO, "Drive" #device "IdleMethod", drive_idle_strings, drive_idle_values }, \
  { NULL, MUI_TYPE_CHECK, "Drive" #device "RAM2000", NULL, NULL }, \
  { NULL, MUI_TYPE_CHECK, "Drive" #device "RAM4000", NULL, NULL }, \
  { NULL, MUI_TYPE_CHECK, "Drive" #device "RAM6000", NULL, NULL }, \
  { NULL, MUI_TYPE_CHECK, "Drive" #device "RAM8000", NULL, NULL }, \
  { NULL, MUI_TYPE_CHECK, "Drive" #device "RAMA000", NULL, NULL }, \
  { NULL, MUI_TYPE_CHECK, "Drive" #device "ParallelCable", NULL, NULL },
#define DECL_NUM (9)

static ui_to_from_t ui_to_from[] = {
  DECL(8) DECL(9) DECL(10) DECL(11)
  UI_END /* mandatory */
};

static APTR build_gui(void)
{
  static const char **drive_type_strings[4] = {
    drive_type_strings_8, drive_type_strings_9, drive_type_strings_10, drive_type_strings_11
  };

  APTR window, page;
  ui_to_from_t *data;
  int num;

  window = RegisterObject,
    MUIA_Register_Titles, drive_number_strings,
  End;

  for (num=0; num<4; num++) {
    data = &ui_to_from[DECL_NUM * num];
    page = GroupObject,
      MUIA_Group_Horiz, TRUE,
      Child, data[0].object = RadioObject,
        MUIA_Frame, MUIV_Frame_Group,
        MUIA_FrameTitle, "Drive type",
        MUIA_Radio_Entries, drive_type_strings[num],
      End,
      Child, GroupObject,
        Child, data[1].object = RadioObject,
          MUIA_Frame, MUIV_Frame_Group,
          MUIA_FrameTitle, "40 track handling",
          MUIA_Radio_Entries, drive_extend_strings,
        End,
        Child, GroupObject,
          MUIA_Frame, MUIV_Frame_Group,
          MUIA_FrameTitle, "Drive expansion",
            CHECK(data[3].object, "$2000-$3FFF RAM")
            CHECK(data[4].object, "$4000-$5FFF RAM")
            CHECK(data[5].object, "$6000-$7FFF RAM")
            CHECK(data[6].object, "$8000-$9FFF RAM")
            CHECK(data[7].object, "$A000-$BFFF RAM")
        End,
      End,
      Child, GroupObject,
        Child, data[2].object = RadioObject,
          MUIA_Frame, MUIV_Frame_Group,
          MUIA_FrameTitle, "Idle method",
          MUIA_Radio_Entries, drive_idle_strings,
        End,
        CHECK(data[8].object, "Parallel cable")
      End,
    End;

    DoMethod(window, OM_ADDMEMBER, page);
  }

  return window;
}

void uidrivec64vic20_settings_dialog(void)
{
  mui_show_dialog(build_gui(), "Drive Settings", ui_to_from);
}
