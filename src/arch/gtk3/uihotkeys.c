/** \file   uihotkeys.c
 * \brief   GTK3 hotkeys dialog
 *
 * Display a list of hotkeys (keyboard shorcuts). At some point those should be
 * (partially) editable.
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
 */


#include "vice.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include "lib.h"
#include "util.h"

#include "debug_gtk3.h"
/* for ui_get_active_window() */
#include "ui.h"
/* for VICE_MOD_MASK */
#include "uimenu.h"

#include "uihotkeys.h"


/** \brief  Maximum number of allowed modifiers
 *
 * This should be plenty, we usually have on or two and three at the most.
 */
#define MAX_MODS    8


/** \brief  Object holding information on a single hotkey
 */
typedef struct hotkey_info_s {
    GdkModifierType mods;   /**< modifiers bitmask */
    const char *key;        /**< key/keys without modifiers */
    const char *desc;       /**< description of the action triggered */
} hotkey_info_t;


/** \brief  List terminator
 */
#define ARNIE   { 0, NULL, NULL }


/*
 * Supported modifier description strings
 */

/** \brief  Label for the control modifier
 */
static const char *mod_control = "Control";

#ifdef ARCHDEP_OS_MACOS
/** \brief  Label for the MOD_VICE modifier ('Option on MacOS)
 */
static const char *mod_vice = "Option";
#else
/** \brief  Label for the MOD_VICE modifier ('Alt' on non-MacOS)
 */
static const char *mod_vice = "Alt";
#endif
/** \brief  Label for the shift modifier
 */
static const char *mod_shift = "Shift";


/** \brief  List of hotkeys
 *
 * At some point we should alter the menu code and the various popup menus to
 * register their hotkeys, rather than using this fixed list. So a single source
 * of these keys would be prefered, probably by implementing some 'actions' list
 * which various menu items and hotkeys can connect to, which should also allow
 * customizing hotkeys.
 * But that's going to need some serious refactoring.
 */
static const hotkey_info_t hotkeys_list[] = {
    /* Quit, pause, reset */
    { VICE_MOD_MASK, "Q", "Quit emulator" },
    { VICE_MOD_MASK, "F9", "Soft reset" },
    { VICE_MOD_MASK, "F12", "Hard reset" },

    /* attach/detach disk/tape */
    { VICE_MOD_MASK, "A", "Smart attach disk/tape image" },
    { VICE_MOD_MASK, "T", "Attach tape image" },
    { VICE_MOD_MASK, "[8, 9, 0, 1]", "Attach disk image to unit #8-11" },
    { GDK_CONTROL_MASK|VICE_MOD_MASK, "[8, 9, 0, 1]", "Detach disk image from unit #8-11" },

    /* printer */
    { VICE_MOD_MASK, "[4, 5, 6]", "Formfeed printer #4-6" },

    /* fliplist */
    { VICE_MOD_MASK, "I", "Add current disk image to fliplist" },
    { VICE_MOD_MASK, "K", "Remove current disk image from fliplist" },
    { VICE_MOD_MASK, "N", "Attach next disk image in fliplist" },
    { VICE_MOD_MASK|GDK_SHIFT_MASK, "N", "Attach previous disk image in fliplist" },

    /* cart */
    { VICE_MOD_MASK, "C", "Attach cartridge image" },
    { VICE_MOD_MASK|GDK_SHIFT_MASK, "C", "Detach cartridge image" },
    { VICE_MOD_MASK, "Z", "Trigger cartridge freeze button" },

    /* pause, frame-advance, warp */
    { VICE_MOD_MASK, "H", "Enter monitor" },
    { VICE_MOD_MASK, "P", "Toggle pause state" },
    { VICE_MOD_MASK|GDK_SHIFT_MASK, "P",
        "Advance one frame (pauses if emulator isn't paused already)" },
    { VICE_MOD_MASK, "W", "Toggle warp mode" },

    /* controllers */
    { VICE_MOD_MASK, "M", "Enable mouse grab" },
    { VICE_MOD_MASK, "J", "Swap joystick port devices" },
    { VICE_MOD_MASK|GDK_SHIFT_MASK, "J", "Swap userport devices" },

    /* media recording/snapshot */
    { VICE_MOD_MASK, "L", "Load snapshot" },
    { VICE_MOD_MASK, "S", "Save snapshot" },
    { VICE_MOD_MASK, "E", "Set milestone" },
    { VICE_MOD_MASK, "U", "Revert to milestone" },
    { VICE_MOD_MASK|GDK_SHIFT_MASK, "R",
        "Start recording media file/make screenshot" },
    { VICE_MOD_MASK|GDK_SHIFT_MASK, "S", "Stop media recording" },
#if 0
    { GDK_SHIFT_MASK|VICE_MOD_MASK, "F12",
        "Take screenshot with autogenerated filename" },
#else
    { 0, "Pause", "Take screenshot with autogenerated filename" },
#endif

    /* others */
    { VICE_MOD_MASK, "D", "Toggle fullscreen mode" },
    { VICE_MOD_MASK, "B", "Toggle fullscreen menu and statusbar" },
    { VICE_MOD_MASK, "R", "Restore active window to normal size" },
    { VICE_MOD_MASK, "O", "Open settings dialog" },
    { VICE_MOD_MASK, "Delete", "Copy BASIC screen text to clipboard" },
    { VICE_MOD_MASK, "Insert", "Paste clipboard to BASIC" },

    ARNIE
};


/** \brief  Hotkeys tree model
 */
static GtkTreeStore *hk_model;


/** \brief  Hotkeys tree view
 */
static GtkWidget *hk_view;


/** \brief  Handler for the 'response' event of the dialog
 *
 * Currently only responds to the 'Close' button.
 *
 * \param[in,out]   dialog      dialog
 * \param[in]       response_id response ID
 * \param[in]       data        extra event data
 */
static void on_response(GtkWidget *dialog, gint response_id, gpointer data)
{
    debug_gtk3("called with response ID %d.", response_id);
    if (response_id == GTK_RESPONSE_REJECT) {
        gtk_widget_destroy(dialog);
    }
}


/** \brief  Generate a string from modifiers bits and key(s) used
 *
 * The \a keys argument is a string to allow documenting closely related actions
 * in a single table row, for example attaching disks: we can use "8,9,0,1"
 * in stead of having four separate entries for Alt+8, Alt+9, Alt+0 and Alt+1.
 *
 * \param[in]   mods    modifier mask as used by GDK
 * \param[in]   keys    key(s) used for the action to trigger
 *
 * \return  string with modifiers as strings and key(s)
 *
 * \note    free with lib_free() after use
 */
static char *modifiers_to_string(GdkModifierType mods, const char *keys)
{
    const char *mod_strings[MAX_MODS + 2];
    int i;

    debug_gtk3("mods = %u.", mods);

    for (i = 0; i < MAX_MODS + 2; i++) {
        mod_strings[i] = NULL;
    }

    for (i = 0; i < MAX_MODS && mods != 0; i++) {
        if (mods & GDK_CONTROL_MASK) {
            mod_strings[i] = mod_control;
            mods ^= GDK_CONTROL_MASK;
        } else if (mods & VICE_MOD_MASK) {
            mod_strings[i] = mod_vice;
            mods ^= VICE_MOD_MASK;
        } else if (mods & GDK_SHIFT_MASK) {
            mod_strings[i] = mod_shift;
            mods ^= GDK_SHIFT_MASK;
        }
    }
    mod_strings[i] = keys;

    return util_strjoin(mod_strings, " + ");
}


/** \brief  Create model for the hotkeys table
 *
 * \return  GtkTreeStore filled with data from \a hotkeys_list
 */
static GtkTreeStore *create_model(void)
{
    GtkTreeStore *model;
    GtkTreeIter iter;
    int i;

    debug_gtk3("building hotkeys model.");
    model = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    for (i = 0; hotkeys_list[i].key != NULL; i++) {
        char *keys = modifiers_to_string(hotkeys_list[i].mods, hotkeys_list[i].key);
        debug_gtk3("modifiers = '%s'.", keys);
        gtk_tree_store_append(model, &iter, NULL);
        gtk_tree_store_set(model, &iter,
                           0, keys,
                           1, hotkeys_list[i].desc,
                           -1);
        lib_free(keys);
    }
    debug_gtk3("added %d hotkeys.", i);
    return model;
}


/** \brief  Create view for the hotkeys table
 *
 * \param[in]   model   model for the view
 *
 * \return  GtkTreeView
 */
static GtkWidget *create_view(GtkTreeStore *model)
{
    GtkWidget *view;
    GtkCellRenderer *text_renderer;
    GtkTreeViewColumn *column_key;
    GtkTreeViewColumn *column_desc;

    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));

    text_renderer = gtk_cell_renderer_text_new();

    column_key = gtk_tree_view_column_new_with_attributes(
            "Hotkey", text_renderer, "text", 0, NULL);
    column_desc = gtk_tree_view_column_new_with_attributes(
            "Action", text_renderer, "text", 1, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column_key);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column_desc);

    return view;
}


/** \brief  Create and show hotkeys table dialog
 *
 * \param[in]   widget  parent widget
 * \param[in]   data    extra event data (unused)
 *
 * \return  TRUE
 */
gboolean uihotkeys_dialog_show(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *content;
    GtkWidget *scrolled;

    hk_model = create_model();
    hk_view = create_view(hk_model);

    content = gtk_grid_new();
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled, 800, 600);
    gtk_container_add(GTK_CONTAINER(scrolled), hk_view);


    dialog = gtk_dialog_new_with_buttons(
            "Hotkeys list",
            ui_get_active_window(),
            GTK_DIALOG_MODAL,
            "Close",
            GTK_RESPONSE_REJECT,
            NULL);

    content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content), scrolled, TRUE, TRUE, 0);

    g_signal_connect_unlocked(dialog, "response", G_CALLBACK(on_response), NULL);
    gtk_widget_show_all(dialog);
    return TRUE;
}
