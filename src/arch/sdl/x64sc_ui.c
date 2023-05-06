/*
 * x64sc_ui.c - Implementation of the C64SC-specific part of the UI.
 *
 * Written by
 *  Hannu Nuotio <hannu.nuotio@tut.fi>
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

#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "c64iec.h"
#include "c64mem.h"
#include "c64rom.h"
#include "c64ui.h"
#include "menu_c64_common_expansions.h"
#include "menu_c64cart.h"
#include "menu_c64hw.h"
#include "menu_common.h"
#include "menu_debug.h"
#include "menu_drive.h"
#include "menu_edit.h"
#include "menu_ethernet.h"
#include "menu_ethernetcart.h"
#include "menu_ffmpeg.h"
#include "menu_help.h"
#include "menu_jam.h"
#include "menu_joyport.h"
#include "menu_joystick.h"
#include "menu_media.h"
#include "menu_midi.h"
#include "menu_monitor.h"
#include "menu_network.h"
#include "menu_printer.h"
#include "menu_reset.h"
#include "menu_sampler.h"
#include "menu_screenshot.h"
#include "menu_settings.h"
#include "menu_sid.h"
#include "menu_snapshot.h"
#include "menu_sound.h"
#include "menu_speed.h"
#include "menu_tape.h"
#include "menu_userport.h"
#include "menu_video.h"
#include "resources.h"
#include "ui.h"
#include "uifonts.h"
#include "uimenu.h"
#include "uistatusbar.h"
#include "vicii.h"
#include "vkbd.h"

static UI_MENU_CALLBACK(toggle_VICIIShowStatusbar_callback)
{
    const char *n;

    n = sdl_ui_menu_toggle_helper(activated, "VICIIShowStatusbar");
    if (n != NULL) {
        uistatusbar_open();
    } else {
        uistatusbar_close();
    }
    return n;
}

static UI_MENU_CALLBACK(pause_callback_wrapper);

static UI_MENU_CALLBACK(Machine_dynmenu_callback)
{
    c64_create_machine_menu();

    return MENU_SUBMENU_STRING;
}

static ui_menu_entry_t x64sc_main_menu[] = {
    { "Autostart image",
      MENU_ENTRY_DIALOG,
      autostart_callback,
      NULL },
    { "Drive",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)drive_menu },
    { "Tape",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)tape_menu },
    { "Cartridge",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)c64cart_menu },
    { "Printer",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)printer_iec_menu },
    { "Machine settings",
      MENU_ENTRY_SUBMENU,
      Machine_dynmenu_callback,
      (ui_callback_data_t)c64sc_hardware_menu },
    { "Video settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)c64sc_video_menu },
    { "Sound settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)sound_output_menu },
    { "Sampler settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)sampler_menu },
    { "Snapshot",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)snapshot_menu },
    { "Save media file",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)media_menu },
    { "Speed settings",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)speed_menu },
    { "Reset",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)reset_menu },
    { "Action on CPU JAM",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)jam_menu },
#ifdef HAVE_NETWORK
    { "Network",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)network_menu },
#endif
    { "Pause",
      MENU_ENTRY_OTHER_TOGGLE,
      pause_callback_wrapper,
      NULL },
    /* Caution: index is hardcoded below */
    { "Advance Frame",
      MENU_ENTRY_OTHER,
      advance_frame_callback,
      NULL },
    { "Monitor",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)monitor_menu },
    /* Caution: index is hardcoded below */
    { "Virtual keyboard",
      MENU_ENTRY_OTHER,
      vkbd_callback,
      NULL },
    { "Statusbar",
      MENU_ENTRY_OTHER_TOGGLE,
      toggle_VICIIShowStatusbar_callback,
      NULL },
#ifdef DEBUG
    { "Debug",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)debug_menu },
#endif
    { "Help",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)help_menu },
    { "Settings management",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)settings_manager_menu },
#ifdef USE_SDL2UI
    { "Edit",
      MENU_ENTRY_SUBMENU,
      submenu_callback,
      (ui_callback_data_t)edit_menu },
#endif
    { "Quit emulator",
      MENU_ENTRY_OTHER,
      quit_callback,
      NULL },
    SDL_MENU_LIST_END
};

#ifdef HAVE_NETWORK
# define MENU_ADVANCE_FRAME_IDX      16
# define MENU_VIRTUAL_KEYBOARD_IDX   18
#else
# define MENU_ADVANCE_FRAME_IDX      15
# define MENU_VIRTUAL_KEYBOARD_IDX   17
#endif
static UI_MENU_CALLBACK(pause_callback_wrapper)
{
    x64sc_main_menu[MENU_ADVANCE_FRAME_IDX].status =
        sdl_pause_state || !sdl_menu_state ? MENU_STATUS_ACTIVE : MENU_STATUS_INACTIVE;
    x64sc_main_menu[MENU_VIRTUAL_KEYBOARD_IDX].status =
        sdl_pause_state ? MENU_STATUS_INACTIVE : MENU_STATUS_ACTIVE;
    return pause_callback(activated, param);
}

static void c64scui_set_menu_params(int index, menu_draw_t *menu_draw)
{
    /* VICII */
    menu_draw->max_text_x = 40;
    menu_draw->color_front = menu_draw->color_default_front = 1;
    menu_draw->color_back = menu_draw->color_default_back = 0;
    menu_draw->color_cursor_back = 6;
    menu_draw->color_cursor_revers = 0;
    menu_draw->color_active_green = 13;
    menu_draw->color_inactive_red = 2;
    menu_draw->color_active_grey = 15;
    menu_draw->color_inactive_grey = 11;

    sdl_ui_set_menu_params = NULL;
}

/** \brief  Pre-initialize the UI before the canvas window gets created
 *
 * \return  0 on success, -1 on failure
 */
int c64scui_init_early(void)
{
    return 0;
}

/** \brief  Initialize the UI
 *
 * \return  0 on success, -1 on failure
 */
int c64scui_init(void)
{
    int has_iec = c64iec_get_active_state();

#ifdef SDL_DEBUG
    fprintf(stderr, "%s\n", __func__);
#endif

    sdl_ui_set_menu_params = c64scui_set_menu_params;

    uijoyport_menu_create(1, 1, 1, 1, 1, 0);
    uiuserport_menu_create(1);
    uisampler_menu_create();
    uicart_menu_create();
    uidrive_menu_create(has_iec);
    uikeyboard_menu_create();
    uipalette_menu_create("VICII", NULL);
    uisid_menu_create();
    uiclockport_rr_mmc_menu_create();
    uiclockport_ide64_menu_create();
    uimedia_menu_create();

    sdl_ui_set_main_menu(x64sc_main_menu);
    sdl_ui_font_init(C64_CHARGEN_NAME, 0, 0x800, 0);
    sdl_vkbd_set_vkbd(&vkbd_c64);

#ifdef HAVE_FFMPEG
    sdl_menu_ffmpeg_init();
#endif

    uistatusbar_realize();
    return 0;
}

void c64scui_shutdown(void)
{
    uisound_output_menu_shutdown();
    uikeyboard_menu_shutdown();
    uisid_menu_shutdown();
    uicart_menu_shutdown();
    uipalette_menu_shutdown();
    uijoyport_menu_shutdown();
    uijoystick_menu_shutdown();
    uiuserport_menu_shutdown();
    uitapeport_menu_shutdown();
    uimedia_menu_shutdown();
#ifdef HAVE_MIDI
    sdl_menu_midi_in_free();
    sdl_menu_midi_out_free();
#endif

#ifdef HAVE_RAWNET
    sdl_menu_ethernet_interface_free();
#endif

#ifdef HAVE_FFMPEG
    sdl_menu_ffmpeg_shutdown();
#endif
    sdl_ui_font_shutdown();
}
