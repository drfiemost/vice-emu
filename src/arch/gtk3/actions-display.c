/** \file   actions-display.c
 * \brief   UI action implementations for display-related settings
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

/*
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

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stddef.h>

#include "debug_gtk3.h"
#include "resources.h"
#include "ui.h"
#include "uiactions.h"
#include "uimenu.h"

#include "actions-display.h"


/** \brief  Toggles fullscreen mode
 *
 * If fullscreen is enabled and there are no window decorations requested for
 * fullscreen mode, the mouse pointer is hidden until fullscreen is disabled.
 *
 * FIXME:   Currently doesn't properly update the fullscreen check menu items
 *          in case of x128: each window can be individually fullscreened but
 *          the check items will be set/unset in both windows' menus.
 */
static void fullscreen_toggle_action(void)
{
    gboolean enabled;
    gint index = ui_get_main_window_index();

    if (index != PRIMARY_WINDOW && index != SECONDARY_WINDOW) {
        return;
    }

    /* flip fullscreen mode */
    enabled = !ui_is_fullscreen();
    ui_set_fullscreen_enabled(enabled);

    ui_set_check_menu_item_blocked_by_action(ACTION_FULLSCREEN_TOGGLE, enabled);
    ui_update_fullscreen_decorations();
}


/** \brief Toggles fullscreen window decorations */
static void fullscreen_decorations_toggle_action(void)
{
    gboolean decorations = !ui_fullscreen_has_decorations();

    resources_set_int("FullscreenDecorations", decorations);

    ui_set_check_menu_item_blocked_by_action(ACTION_FULLSCREEN_DECORATIONS_TOGGLE,
                                             decorations);
    ui_update_fullscreen_decorations();
}


/** \brief  Attempt to restore the active window's size to its "natural" size
 *
 * Also unmaximizes and unfullscreens the window.
 */
static void restore_display_action(void)
{
    GtkWindow *window = ui_get_active_window();

    if (window != NULL) {
        /* disable fullscreen if active */
        if (ui_is_fullscreen()) {
            ui_action_trigger(ACTION_FULLSCREEN_TOGGLE);
        }
        /* unmaximize */
        gtk_window_unmaximize(window);
        /* requesting a 1x1 window forces the window to resize to its natural
         * size, ie the minimal size required to display the window's
         * decorations and contents without wasting space
         */
        gtk_window_resize(window, 1, 1);
    }
}


/** \brief  List of display-related actions */
static const ui_action_map_t display_actions[] = {
    {
        .action = ACTION_FULLSCREEN_TOGGLE,
        .handler = fullscreen_toggle_action,
    },
    {
        .action = ACTION_FULLSCREEN_DECORATIONS_TOGGLE,
        .handler = fullscreen_decorations_toggle_action
    },
    {
        .action = ACTION_RESTORE_DISPLAY,
        .handler = restore_display_action
    },

    UI_ACTION_MAP_TERMINATOR
};


/** \brief  Register display-related UI actions */
void actions_display_register(void)
{
    ui_actions_register(display_actions);
}
