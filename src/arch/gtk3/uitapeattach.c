/** \file   uitapeattach.c
 * \brief   GTK3 tape-attach dialog
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 * \author  Michael C. Martin <mcmartin@gmail.com>
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

#include "attach.h"
#include "autostart.h"
#include "tape.h"
#include "debug_gtk3.h"
#include "basedialogs.h"
#include "contentpreviewwidget.h"
#include "filechooserhelpers.h"
#include "imagecontents.h"
#include "lastdir.h"
#include "resources.h"
#include "tapecontents.h"
#include "ui.h"
#include "uimachinewindow.h"

#include "uitapeattach.h"


#ifndef SANDBOX_MODE
/** \brief  File type filters for the dialog
 */
static ui_file_filter_t filters[] = {
    { "Tape images", file_chooser_pattern_tape },
    { "All files", file_chooser_pattern_all },
    { NULL, NULL }
};

/** \brief  Reference to the preview widget
 */
static GtkWidget *preview_widget = NULL;
#endif


/** \brief  Last directory used
 *
 * When a taoe is attached, this is set to the directory of that file. Since
 * it's heap-allocated by Gtk3, it must be freed with a call to
 * ui_tape_attach_shutdown() on emulator shutdown.
 */
static gchar *last_dir = NULL;
static gchar *last_file = NULL;

/** \brief  Reference to the custom 'Autostart' button
 */
static GtkWidget *autostart_button;

#ifndef SANDBOX_MODE
/** \brief  Handler for the "update-preview" event
 *
 * \param[in]   chooser file chooser dialog
 * \param[in]   data    extra event data (unused)
 */
static void on_update_preview(GtkFileChooser *chooser, gpointer data)
{
    GFile *file;
    gchar *path;

    file = gtk_file_chooser_get_preview_file(chooser);
    if (file != NULL) {
        path = g_file_get_path(file);
        if (path != NULL) {
            gchar *path_locale = file_chooser_convert_to_locale(path);

            debug_gtk3("called with '%s'.", path);

            content_preview_widget_set_image(preview_widget, path_locale);
            g_free(path);
            g_free(path_locale);
        }
        g_object_unref(file);
    }
}


/** \brief  Handler for the 'toggled' event of the 'show hidden files' checkbox
 *
 * \param[in]   widget      checkbox triggering the event
 * \param[in]   user_data   data for the event (the dialog)
 */
static void on_hidden_toggled(GtkWidget *widget, gpointer user_data)
{
    int state;

    state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    debug_gtk3("show hidden files: %s.", state ? "enabled" : "disabled");

    gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(user_data), state);
}
#endif

/** \brief  Trigger autostart
 *
 * \param[in]   widget  dialog
 * \param[in]   data    file index in the directory preview
 */
static void do_autostart(GtkWidget *widget, int index, int autostart)
{
    gchar *filename;
    gchar *filename_locale;

    lastdir_update(widget, &last_dir, &last_file);
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
    debug_gtk3("Autostarting file '%s'.", filename);

    filename_locale = file_chooser_convert_to_locale(filename);
    if (autostart_tape(
                filename_locale,
                NULL,   /* program name */
                index,
                autostart ? AUTOSTART_MODE_RUN : AUTOSTART_MODE_LOAD) < 0) {
        /* oeps */
        debug_gtk3("autostart tape attach failed.");
    }
    g_free(filename_locale);
}

/** \brief  attach image
 *
 * \param[in]   widget  dialog
 * \param[in]   data    file index in the directory preview
 */
static void do_attach(GtkWidget *widget, gpointer user_data)
{
    gchar *filename;
    gchar *filename_locale;

    lastdir_update(widget, &last_dir, &last_file);
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
    /* ui_message("Opening file '%s' ...", filename); */
    debug_gtk3("Attaching file '%s' to tape unit.", filename);

    filename_locale = file_chooser_convert_to_locale(filename);

    if (tape_image_attach(1, filename_locale) < 0) {
        /* failed */
        debug_gtk3("tape attach failed.");
    }
    g_free(filename_locale);
}

/** \brief  Handler for 'selection-changed' event of the preview widget
 *
 * Checks if a proper selection was made and activates the 'Autostart' button
 * if so, disabled it otherwise
 *
 * \param[in]   chooser Parent dialog
 * \param[in]   data    Extra event data (unused)
 */
static void on_selection_changed(GtkFileChooser *chooser, gpointer data)
{
    gchar *filename;

    filename = gtk_file_chooser_get_filename(chooser);
    if (filename != NULL) {
        gtk_widget_set_sensitive(autostart_button, TRUE);
        g_free(filename);
    } else {
        gtk_widget_set_sensitive(autostart_button, FALSE);
    }
}

#ifndef SANDBOX_MODE

/** \brief  Handler for 'response' event of the dialog
 *
 * This handler is called when the user clicks a button in the dialog.
 *
 * \param[in]   widget      the dialog
 * \param[in]   response_id response ID
 * \param[in]   user_data   index in the preview widget
 *
 * TODO:    proper (error) messages, which requires implementing ui_error() and
 *          ui_message() and moving them into gtk3/widgets to avoid circular
 *          references
 */
static void on_response(GtkWidget *widget, gint response_id, gpointer user_data)
{
    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
    int index = content_preview_widget_get_index(preview_widget);
    int autostart = 0;

#ifdef HAVE_DEBUG_GTK3UI
    debug_gtk3("on_response, got response ID %d, index %d\n", response_id, index);
#endif
    resources_get_int("AutostartOnDoubleclick", &autostart);
    debug_gtk3("on_response, AutostartOnDoubleclick = %s\n",
            autostart ? "True" : "False");

    /* first, to make the following logic less funky, map some events to others,
       depending on whether autostart-on-doubleclick is enabled or not, and 
       depending on the event coming from the preview window or not. */
    switch (response_id) {
        /* double-click on file in the preview widget when autostart-on-doubleclick is NOT enabled */
        case VICE_RESPONSE_AUTOLOAD_INDEX:
        /* double-click on file in the preview widget when autostart-on-doubleclick is enabled */
        case VICE_RESPONSE_AUTOSTART_INDEX:
            if ((index < 0) || (filename == NULL)) {
                response_id = VICE_RESPONSE_INVALID;   /* drop this event */
            }
            break;
        /* 'Open' button clicked when autostart-on-doubleclick is enabled */
        case VICE_RESPONSE_CUSTOM_OPEN:
            if (filename == NULL) {
                response_id = VICE_RESPONSE_INVALID;   /* drop this event */
            } else if (index >= 0) {
                response_id = VICE_RESPONSE_AUTOLOAD_INDEX;
            }
            break;
        /* double-click on file in the main file chooser,
           'Autostart' button when autostart-on-doubleclick is enabled
           'Open' button when autostart-on-doubleclick is NOT enabled */
        case GTK_RESPONSE_ACCEPT:
            debug_gtk3("on_response, GTK_RESPONSE_ACCEPT, index %d\n", index);
            if (filename == NULL) {
                response_id = VICE_RESPONSE_INVALID;   /* drop this event */
            } else if ((index >= 0) && (autostart == 0)) {
                response_id = VICE_RESPONSE_AUTOLOAD_INDEX;
            } else if ((index >= 0) && (autostart == 1)) {
                response_id = VICE_RESPONSE_AUTOSTART_INDEX;
            } else if (autostart == 0) {
                response_id = VICE_RESPONSE_CUSTOM_OPEN;
            } else {
                response_id = VICE_RESPONSE_AUTOSTART;
            }
            break;
        default:
            break;
    }

#ifdef HAVE_DEBUG_GTK3UI
    debug_gtk3("on_response, using response ID %d\n", response_id);
#endif
    switch (response_id) {
        /* 'Open' button clicked when autostart-on-doubleclick is enabled */
        case VICE_RESPONSE_CUSTOM_OPEN:
            debug_gtk3("on_response, VICE_RESPONSE_CUSTOM_OPEN, index %d\n", index);
            do_attach(widget, user_data);

            gtk_widget_destroy(widget);
            break;

        /* 'Autostart' button clicked when autostart-on-doubleclick is NOT enabled */
        case VICE_RESPONSE_AUTOSTART:
        /* double-click on file in the preview widget when autostart-on-doubleclick is enabled */
        case VICE_RESPONSE_AUTOSTART_INDEX:
            debug_gtk3("on_response, VICE_RESPONSE_AUTOSTART_INDEX, index %d\n", index);
            do_autostart(widget, index + 1, 1);

            gtk_widget_destroy(widget);
            break;

        /* double-click on file in the preview widget when autostart-on-doubleclick is NOT enabled */
        case VICE_RESPONSE_AUTOLOAD_INDEX:
            debug_gtk3("on_response, VICE_RESPONSE_AUTOLOAD_INDEX, index %d\n", index);
            do_autostart(widget, index + 1, 0);

            gtk_widget_destroy(widget);
            break;

        /* 'Close'/'X' button */
        case GTK_RESPONSE_REJECT:
            gtk_widget_destroy(widget);
            break;
        default:
            break;
    }

    if (filename != NULL) {
        g_free(filename);
    }
}


#else

/** \brief  Handler for 'response' event of the dialog (sandbox mode)
 *
 * This handler is called when the user clicks a button in the dialog.
 *
 * \param[in]   widget      the dialog
 * \param[in]   response_id response ID
 * \param[in]   user_data   extra data (unused)
 *
 * TODO:    proper (error) messages, which requires implementing ui_error() and
 *          ui_message() and moving them into gtk3/widgets to avoid circular
 *          references
 */
static void on_response_native(GtkFileChooserNative *widget,
                               gint response_id,
                               gpointer user_data)
{
    gchar *filename;
    gchar *filename_locale;
    int index;

    index = GPOINTER_TO_INT(user_data);

    debug_gtk3("got response ID %d, index %d.", response_id, index);

    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));

    switch (response_id) {

        /* 'Open' button, double-click on file */
        case GTK_RESPONSE_ACCEPT:
            /* ui_message("Opening file '%s' ...", filename); */
            debug_gtk3("Attaching file '%s' to tape unit.", filename);

            filename_locale = file_chooser_convert_to_locale(filename);

            if (tape_image_attach(1, filename_locale) < 0) {
                /* failed */
                debug_gtk3("tape attach failed.");
            }
            g_free(filename_locale);
            gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(widget));
            break;
#if 0
        /* 'Autostart' button clicked */
        case VICE_RESPONSE_AUTOSTART:
            lastdir_update(widget, &last_dir);
            if (filename != NULL) {
                debug_gtk3("Autostarting file '%s'.", filename);

                filename_locale = file_chooser_convert_to_locale(filename);
                if (autostart_tape(
                            filename_locale,
                            NULL,   /* program name */
                            index,
                            AUTOSTART_MODE_RUN) < 0) {
                    /* oeps */
                    debug_gtk3("autostart tape attach failed.");
                }
                g_free(filename_locale);
                gtk_widget_destroy(widget);
            }
            break;
#endif
        /* 'Close'/'X' button */
        case GTK_RESPONSE_REJECT:
            gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(widget));
            break;
        default:
            break;
    }

    if (filename != NULL) {
        g_free(filename);
    }
}
#endif


#ifndef SANDBOX_MODE
/** \brief  Create the 'extra' widget
 *
 * \param[in]   parent  parent widget
 *
 * \return  GtkGrid
 *
 * TODO: 'grey-out'/disable units without a proper drive attached
 */
static GtkWidget *create_extra_widget(GtkWidget *parent)
{
    GtkWidget *grid;
    GtkWidget *hidden_check;
#if 0
    GtkWidget *readonly_check;
#endif

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    hidden_check = gtk_check_button_new_with_label("Show hidden files");
    g_signal_connect(hidden_check, "toggled", G_CALLBACK(on_hidden_toggled),
            (gpointer)(parent));
    gtk_grid_attach(GTK_GRID(grid), hidden_check, 0, 0, 1, 1);

#if 0
    readonly_check = gtk_check_button_new_with_label("Attach read-only");
    gtk_grid_attach(GTK_GRID(grid), readonly_check, 1, 0, 1, 1);
#endif

    gtk_widget_show_all(grid);
    return grid;
}
#endif


#ifndef SANDBOX_MODE
/** \brief  Create the tape attach dialog
 *
 * \param[in]   parent  parent widget, used to get the top level window
 *
 * \return  GtkFileChooserDialog
 */
static GtkWidget *create_tape_attach_dialog(GtkWidget *parent)
{
    GtkWidget *dialog;
    size_t i;
    int autostart = 0;

    resources_get_int("AutostartOnDoubleclick", &autostart);
    debug_gtk3("create_smart_attach_dialog, AutostartOnDoubleclick = %s\n",
            autostart ? "True" : "False");

    /* create new dialog */
    dialog = gtk_file_chooser_dialog_new(
            "Attach a tape image",
            ui_get_active_window(),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            /* buttons */
            NULL, NULL);

    /* We need to manually add the buttons here, not using the constructor
     * above, to keep the order of the buttons the same as in the other
     * 'attach' dialogs. Gtk doesn't allow getting references to buttons when
     * added via the constructor, meaning we cannot get a reference to the
     * "Autostart" button in order to "grey it out".
     */
    /* to handle the "double click means autostart" option, we need to always
       connect a button to GTK_RESPONSE_ACCEPT, else double clicks will stop
       working */
    if (autostart) {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Attach / Load", VICE_RESPONSE_CUSTOM_OPEN);
        autostart_button = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                                "Autostart",
                                                GTK_RESPONSE_ACCEPT);
    } else {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Attach / Load", GTK_RESPONSE_ACCEPT);
        autostart_button = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                                "Autostart",
                                                VICE_RESPONSE_AUTOSTART);
    }

    gtk_widget_set_sensitive(autostart_button, FALSE);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Close", GTK_RESPONSE_REJECT);

    /* set modal so mouse-grab doesn't get triggered */
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    /* set last directory */
    lastdir_set(dialog, &last_dir, &last_file);

    /* add 'extra' widget: 'readonly' and 'show preview' checkboxes */
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog),
                                      create_extra_widget(dialog));

    preview_widget = content_preview_widget_create(dialog, tapecontents_read,
            on_response);
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog),
            preview_widget);

    /* add filters */
    for (i = 0; filters[i].name != NULL; i++) {
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),
                create_file_chooser_filter(filters[i], FALSE));
    }

    /* connect "reponse" handler: the `user_data` argument gets filled in when
     * the "response" signal is emitted: a response ID */
    g_signal_connect(dialog, "response", G_CALLBACK(on_response), NULL);
    g_signal_connect(dialog, "update-preview", G_CALLBACK(on_update_preview),
            NULL);
    g_signal_connect_unlocked(dialog, "selection-changed",
            G_CALLBACK(on_selection_changed), NULL);

    return dialog;

}

#else

/** \brief  Create the tape attach dialog (sandbox version)
 *
 * \param[in]   parent  parent widget, used to get the top level window
 *
 * \return  GtkFileChooserNative
 */
static GtkFileChooserNative *create_tape_attach_dialog_native(GtkWidget *parent)
{
    GtkFileChooserNative *dialog;

    /* create new dialog */
    dialog = gtk_file_chooser_native_new(
            "Attach a tape image",
            ui_get_active_window(),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            /* buttons */
            NULL, NULL);
#if 0
    /* set modal so mouse-grab doesn't get triggered */
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    /* set last directory */
    lastdir_set(dialog, &last_dir);

    /* add 'extra' widget: 'readonly' and 'show preview' checkboxes */
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog),
                                      create_extra_widget(dialog));

    preview_widget = content_preview_widget_create(dialog, tapecontents_read,
            on_response);
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog),
            preview_widget);

    /* add filters */
    for (i = 0; filters[i].name != NULL; i++) {
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),
                create_file_chooser_filter(filters[i], FALSE));
    }
#endif
    /* connect "reponse" handler: the `user_data` argument gets filled in when
     * the "response" signal is emitted: a response ID */
    g_signal_connect(dialog, "response", G_CALLBACK(on_response_native), NULL);

    return dialog;
}
#endif


/** \brief  Callback for the "attach tape image" menu items
 *
 * Creates the dialog and runs it.
 *
 * \param[in]   widget      menu item triggering the callback
 * \param[in]   user_data   ignored
 *
 * \return  TRUE (signal to Gtk the event was 'consumed')
 */
gboolean ui_tape_attach_callback(GtkWidget *widget, gpointer user_data)
{
#ifndef SANDBOX_MODE
    GtkWidget *dialog;
    debug_gtk3("called.");
    dialog = create_tape_attach_dialog(widget);
    gtk_widget_show(dialog);
#else
    GtkFileChooserNative *dialog;
    dialog = create_tape_attach_dialog_native(widget);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
#endif
    return TRUE;
}


/** \brief  Callback for "detach tape image" menu items
 *
 * Removes any tape from the specified drive. No additional UI is
 * presented.
 *
 * \param[in]   widget      menu item triggering the callback
 * \param[in]   user_data   ignored
 *
 * \return  TRUE
 */
gboolean ui_tape_detach_callback(GtkWidget *widget, gpointer user_data)
{
    tape_image_detach(1);
    return TRUE;
}


/** \brief  Clean up the last directory string
 */
void ui_tape_attach_shutdown(void)
{
    lastdir_shutdown(&last_dir, &last_file);
}
