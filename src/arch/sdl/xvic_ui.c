/*
 * xvic_ui.c - Implementation of the VIC20-specific part of the UI.
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
#include "actions-display.h"
#include "actions-speed.h"
#include "lib.h"
#include "machine.h"
#include "menu_common.h"
#include "menu_debug.h"
#include "menu_drive.h"
#include "menu_edit.h"
#include "menu_ethernet.h"
#include "menu_ffmpeg.h"
#include "menu_help.h"
#include "menu_jam.h"
#include "menu_joyport.h"
#include "menu_joystick.h"
#include "menu_log.h"
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
#include "menu_vic20cart.h"
#include "menu_vic20hw.h"
#include "menu_video.h"
#include "resources.h"
#include "ui.h"
#include "uiactions.h"
#include "uifonts.h"
#include "uimenu.h"
#include "uistatusbar.h"
#include "vic.h"
#include "victypes.h"
#include "vic20rom.h"
#include "vic20ui.h"
#include "vic20memrom.h"
#include "videoarch.h"
#include "vkbd.h"


static ui_menu_entry_t xvic_main_menu[] = {
    {   .action    = ACTION_SMART_ATTACH,
        .string    = "Autostart image",
        .type      = MENU_ENTRY_DIALOG,
        .callback  = autostart_callback,
        .activated = MENU_EXIT_UI_STRING
    },
    {   .string   = "Drive",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)drive_menu
    },
    {   .string   = "Tape",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)tape_menu
    },
    {   .string   = "Cartridge",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)vic20cart_menu
    },
    {   .string   = "Printer",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)printer_iec_menu
    },
    {   .string   = "Machine settings",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)vic20_hardware_menu
    },
    {   .string   = "Video settings",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)vic20_video_menu
    },
    {   .string   = "Sound settings",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)sound_output_menu
    },
    {   .string   = "Sampler settings",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)sampler_menu
    },
    {   .string   = "Snapshot",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)snapshot_menu
    },
    {   .string   = "Save media file",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)media_menu
    },
    {   .string   = "Speed settings",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)speed_menu
    },
    {   .string   = "Reset",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)reset_menu
    },
    {   .string   = "Action on CPU JAM",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)jam_menu
    },
#ifdef HAVE_NETWORK
    {   .string   = "Network",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)network_menu
    },
#endif
    {   .action    = ACTION_PAUSE_TOGGLE,
        .string    = "Pause",
        .type      = MENU_ENTRY_OTHER_TOGGLE,
        .displayed = pause_toggle_display
    },
    {   .action   = ACTION_ADVANCE_FRAME,
        .string   = "Advance Frame",
        .type     = MENU_ENTRY_OTHER,
    },
    {   .string   = "Monitor",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)monitor_menu
    },
    {   .action    = ACTION_VIRTUAL_KEYBOARD,
        .string    = "Virtual keyboard",
        .type      = MENU_ENTRY_OTHER,
        .activated = MENU_EXIT_UI_STRING
    },
    {   .action    = ACTION_SHOW_STATUSBAR_TOGGLE,
        .string    = "Statusbar",
        .type      = MENU_ENTRY_OTHER_TOGGLE,
        .displayed = show_statusbar_toggle_display
    },
#ifdef DEBUG
    {   .string   = "Debug",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)debug_menu
    },
#endif
    {   .string   = "Help",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)help_menu
    },
    {   .string   = "Settings management",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)settings_manager_menu
    },
    {   .string   = "Log settings",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)log_menu
    },
#ifdef USE_SDL2UI
    {   .string   = "Edit",
        .type     = MENU_ENTRY_SUBMENU,
        .callback = submenu_callback,
        .data     = (ui_callback_data_t)edit_menu
    },
#endif
    {   .action   = ACTION_QUIT,
        .string   = "Quit emulator",
        .type     = MENU_ENTRY_OTHER,
    },
    SDL_MENU_LIST_END
};

static void vic20ui_set_menu_params(int index, menu_draw_t *menu_draw)
{
    int videostandard;

    resources_get_int("MachineVideoStandard", &videostandard);

    /* VIC */
#ifdef VIC_DUPLICATES_PIXELS
    menu_draw->max_text_x = 48;
#else
    /* FIXME some functions assume max_text_x to be at least 40 */
    menu_draw->max_text_x = (videostandard == MACHINE_SYNC_PAL) ? 28 : 25;
#endif
    menu_draw->max_text_y = 26;

    menu_draw->extra_x = sdl_active_canvas->viewport->first_x - sdl_active_canvas->geometry->gfx_position.x;
    menu_draw->extra_y = sdl_active_canvas->geometry->first_displayed_line - sdl_active_canvas->geometry->gfx_position.y;

#ifdef VIC_DUPLICATES_PIXELS
    menu_draw->extra_x += (videostandard == MACHINE_SYNC_PAL) ? 36 : 8;
#endif
    menu_draw->extra_y += (videostandard == MACHINE_SYNC_PAL) ? 40 : 24;

    menu_draw->color_front = menu_draw->color_default_front = 1;
    menu_draw->color_back = menu_draw->color_default_back = 0;
    menu_draw->color_cursor_back = 6;
    menu_draw->color_cursor_revers = 0;
    menu_draw->color_active_green = 13;
    menu_draw->color_inactive_red = 2;
    menu_draw->color_active_grey = 15;
    menu_draw->color_inactive_grey = 11;
}

/** \brief  Pre-initialize the UI before the canvas window gets created
 *
 * \return  0 on success, -1 on failure
 */
int vic20ui_init_early(void)
{
    return 0;
}

/** \brief  Initialize the UI
 *
 * \return  0 on success, -1 on failure
 */
int vic20ui_init(void)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s\n", __func__);
#endif

    sdl_ui_set_menu_params = vic20ui_set_menu_params;
    uijoyport_menu_create(1, 0, 1, 1, 1, 0);
    uiuserport_menu_create(1);
    uisampler_menu_create();
    uidrive_menu_create(1);
    uitape_menu_create(1);
    uikeyboard_menu_create();
    uipalette_menu_create("VIC", NULL);
    uisid_menu_create();
    uimedia_menu_create();

    sdl_ui_set_main_menu(xvic_main_menu);
    sdl_ui_font_init(VIC20_CHARGEN_NAME, 0, 0x800, 0);
    sdl_vkbd_set_vkbd(&vkbd_vic20);

    sdl_menu_ffmpeg_init();

    uistatusbar_realize();
    return 0;
}

void vic20ui_shutdown(void)
{
    uisound_output_menu_shutdown();
    uisid_menu_shutdown();
    uikeyboard_menu_shutdown();
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

    sdl_menu_ffmpeg_shutdown();

    sdl_ui_font_shutdown();
}
