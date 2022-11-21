/** \file   vsidplaylistwidget.c
 * \brief   GTK3 playlist widget for VSID
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

/*
 * Icons used by this file:
 *
 * $VICEICON    actions/media-skip-backward
 * $VICEICON    actions/media-seek-backward
 * $VICEICON    actions/media-seek-forward
 * $VICEICON    actions/media-skip-forward
 * $VICEICON    status/media-playlist-repeat
 * $VICEICON    status/media-playlist-shuffle
 * $VICEICON    actions/list-add
 * $VICEICON    actions/list-remove
 * $VICEICON    actions/document-open
 * $VICEICON    actions/document-save
 * $VICEICON    actions/edit-clear-all
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
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#include "archdep_get_hvsc_dir.h"
#include "hvsc.h"
#include "lastdir.h"
#include "lib.h"
#include "m3u.h"
#include "resources.h"
#include "uiapi.h"
#include "uivsidwindow.h"
#include "util.h"
#include "version.h"
#ifdef USE_SVN_REVISION
#include "svnversion.h"
#endif
#include "vice_gtk3.h"
#include "vsidplaylistadddialog.h"
#include "vsidtuneinfowidget.h"

#include "vsidplaylistwidget.h"


/* \brief  Control button types */
enum {
    BUTTON_PUSH,    /**< action button: simple push button */
    BUTTON_TOGGLE     /**< toggle button: for 'repeat' and 'shuffle' */
};

/* Playlist column indexes */
enum {
    COL_TITLE,          /**< title */
    COL_AUTHOR,         /**< author */
    COL_FULL_PATH,      /**< full path to psid file */
    COL_DISPLAY_PATH,   /**< displayed path (HVSC stripped if possible) */

    NUM_COLUMNS         /**< number of columns in the model */
};

/** \brief  Playlist control button struct
 */
typedef struct plist_ctrl_button_s {
    const char *icon_name;  /**< icon-name in the Gtk3 theme */
    int         type;       /**< type of button (action/toggle) */
    void        (*callback)(GtkWidget *, gpointer); /**< callback function */
    const char *tooltip;    /**< tooltip text */
} plist_ctrl_button_t;

/** \brief  Type of context menu item types
 */
typedef enum {
    CTX_MENU_ACTION,    /**< action */
    CTX_MENU_SEP        /**< menu separator */
} ctx_menu_item_type_t;

/** \brief  Context menu item object
 */
typedef struct ctx_menu_item_s {
    const char *            text;   /**< displayed text */
    /**< item callback */
    gboolean                (*callback)(GtkWidget *, gpointer);
    ctx_menu_item_type_t    type;   /**< menu item type, \see ctx_menu_item_type_t */
} ctx_menu_item_t;

/** \brief  VSID Hotkey info object
 */
typedef struct vsid_hotkey_s {
    guint keyval;                                   /**< GDK key value */
    guint modifiers;                                /**< GDK modifiers */
    gboolean (*callback)(GtkWidget *, gpointer);    /**< hotkey callback */
} vsid_hotkey_t;


/*
 * Forward declarations
 */
static void update_title(void);


/** \brief  Reference to the playlist model
 */
static GtkListStore *playlist_model;

/** \brief  Reference to the playlist view
 */
static GtkWidget *playlist_view;

/** \brief  Playlist title widget
 *
 * Gets updated with the number of tunes in the list.
 */
static GtkWidget *title_widget;

/** \brief  Reference to the save-playlist dialog's entry to set playlist title
 *
 * Only valid during lifetime of the save-playlist dialog
 */
static GtkWidget *playlist_title_entry;

/** \brief  Playlist title
 *
 * Set when loading an m3u file with the "\#PLAYLIST:" directive.
 * Freed when the main widget is destroyed.
 */
static char *playlist_title;

/** \brief  Playlist path
 *
 * Used to suggest filename when saving playlist, set when loading a playlist.
 */
static char *playlist_path;

/** \brief  Playlist dialogs last-used directory
 *
 * Last used directory for playlist dialogs, used to set the directory of the
 * playlist dialogs.
 */
static char *playlist_last_dir;


/* {{{ Utility functions */

/** \brief  Strip HVSC base dir from \a path
 *
 * Try to strip the HVSC base directory from \a path, otherwise return the
 * full \a path.
 *
 * \param[in]   path    full path to psid file
 *
 * \return  stripped path
 * \note    Free with g_free()
 */
static gchar *strip_hvsc_base(const gchar *path)
{
    const char *hvsc_base = archdep_get_hvsc_dir();

    if (hvsc_base != NULL && *hvsc_base != '\0' &&
            g_str_has_prefix(path, hvsc_base)) {
        /* skip base */
        path += strlen(hvsc_base);
        /* skip directory separator if present */
        if (*path == '/' || *path == '\\') {
            path++;
        }
    }
    return g_strdup(path);
}

/** \brief  Free playlist title
 */
static void vsid_playlist_free_title(void)
{
    if (playlist_title != NULL) {
        lib_free(playlist_title);
    }
    playlist_title = NULL;
}

/** \brief  Set playlist title
 *
 * \param[in]   title   playlist title
 */
static void vsid_playlist_set_title(const char *title)
{
    vsid_playlist_free_title();
    playlist_title = lib_strdup(title);
}

/** \brief  Get playlist title
 *
 * \return  playlist title
 */
static const char *vsid_playlist_get_title(void)
{
    return playlist_title;
}

/** \brief  Free playlist file path
 */
static void vsid_playlist_free_path(void)
{
    if (playlist_path != NULL) {
        lib_free(playlist_path);
    }
    playlist_path = NULL;
}

/** \brief  Set playlist path
 *
 * \param[in]   path    path to playlist file
 */
static void vsid_playlist_set_path(const char *path)
{
    vsid_playlist_free_path();
    playlist_path = lib_strdup(path);
}

/** \brief  Get playlist file path
 *
 * \return  playlist file path
 */
static const char *vsid_playlist_get_path(void)
{
    return playlist_path;
}

/** \brief  Set default directory of playlist load/save dialogs
 *
 * If the default directory (#playlist_last_dir) isn't set yet, we use the
 * HVSC base directory.
 */
static void set_playlist_dialogs_default_dir(void)
{
    if (playlist_last_dir == NULL) {
        const gchar *base = archdep_get_hvsc_dir();
        if (base != NULL) {
            /* the lastdir.c code uses GLib for memory management, so we use
             * g_strdup() here: */
            playlist_last_dir = g_strdup(base);
        }
    }
}

/** \brief  Scroll view so the selected row is visible
 *
 * \param[in]   iter    tree view iter
 *
 * \note    This function assumes the iter is valid since there's no quick
 *          way to check if the iter is valid
 */
static void scroll_to_iter(GtkTreeIter *iter)
{
    GtkTreePath *path;

    path = gtk_tree_model_get_path(GTK_TREE_MODEL(playlist_model), iter);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(playlist_view),
                                 path,
                                 NULL,      /* no column since we provide path */
                                 FALSE,     /* don't align, just do the minimum */
                                 0.0, 0.0); /* alignments, ignored */
    gtk_tree_path_free(path);
}

/* }}} */


/** \brief  Add SID files to the playlist
 *
 * \param[in,out]   files   List of selected files
 *
 * WARNING: this function frees its argument and probably shouldn't do that!
 *
 */
static void add_files_callback(GSList *files)
{
    GSList *pos = files;

    if (files != NULL) {
        do {
            const char *path = (const char *)(pos->data);
            vsid_playlist_widget_append_file(path);
            pos = g_slist_next(pos);
        } while (pos != NULL);
        g_slist_free(files);
    }
}

/* {{{ Context menu callbacks */
/** \brief  Delete selected rows
 *
 * \param[in]   widget  widget triggering the event
 * \param[in]   data    extra event data (unused)
 *
 * \return  FALSE on error
 */
static gboolean on_ctx_delete_selected_rows(GtkWidget *widget, gpointer data)
{
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GList *rows;
    GList *elem;

    /* get model in the correct type for gtk_tree_model_get_iter(),
     * taking the address of GTK_TREE_MODEL(M) doesn't work.
     */
    model = GTK_TREE_MODEL(playlist_model);
    /* get current selection */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
    /* get rows in the selection, which is a GList of GtkTreePath *'s */
    rows = gtk_tree_selection_get_selected_rows(selection, &model);

    /* iterate the list of rows in reverse order to avoid
     * invalidating the GtkTreePath*'s in the list
     */
    for (elem = g_list_last(rows); elem != NULL; elem = elem->prev) {
        GtkTreePath *path;
        GtkTreeIter iter;

        /* delete row */
        path = elem->data;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(playlist_model),
                                    &iter,
                                    path)) {
            gtk_list_store_remove(playlist_model, &iter);
        } else {
            return FALSE;
        }
    }
    g_list_free(rows);
    return TRUE;
}
/* }}} */

/** \brief  Delete the entire playlist
 *
 * \param[in]   widget  widget triggering the callback (unused)
 * \param[in]   data    event reference (unused)
 *
 * \return  TRUE
 */
static gboolean delete_all_rows(GtkWidget *widget, gpointer data)
{
    gtk_list_store_clear(playlist_model);
    return TRUE;
}

/** \brief  Callback for the Insert hotkey
 *
 * \param[in]   widget  widget triggering the callback (unused)
 * \param[in]   data    event reference (unused)
 *
 * \return  TRUE
 */
static gboolean open_add_dialog(GtkWidget *widget, gpointer data)
{
    vsid_playlist_add_dialog_exec(add_files_callback);
    return TRUE;
}


/* {{{ Playlist loading */

/** \brief  M3U entry handler
 *
 * Called by the m3u parser when encountering a normal entry.
 *
 * \param[in]   text    entry text
 * \param[in]   len     length of \a text
 *
 * \return  `false` to stop the parser on an error
 */
static bool playlist_entry_handler(const char *text, size_t len)
{
    vsid_playlist_widget_append_file(util_skip_whitespace(text));
    return true;
}

/** \brief  M3U directive handler
 *
 * Called by the m3u parser when encountering an extended m3u directive.
 *
 * \param[in]   id      directive ID
 * \param[in]   text    text following the directive
 * \param[in]   len     length of \a text
 *
 * \return  `false` to stop the parser on an error
 */
static bool playlist_directive_handler(m3u_ext_id_t id, const char *text, size_t len)
{
    const char *title;

    switch (id) {
        case M3U_EXTM3U:
            debug_gtk3("HEADER: Valid M3Uext 1.1 file");
            break;
        case M3U_PLAYLIST:
            /* make copy of title, the text pointer is invalidated on the
             * next line the parser reads */
            title = util_skip_whitespace(text);
            /* only set title when not empty and not previously set */
            if (*title != '\0' && playlist_title == NULL) {
                vsid_playlist_set_title(title);
            }
            break;
        default:
            break;
    }
    return true;
}

/** \brief  Callback for the load-playlist dialog
 *
 * \param[in]   dialog      load-playlist dialog
 * \param[in]   filename    filename or `NULL` when canceled
 * \param[in]   data        extra callback data (ignored)
 */
static void playlist_load_callback(GtkDialog *dialog,
                                   gchar *filename,
                                   gpointer data)
{
    if (filename != NULL) {
        char buf[1024];

        lastdir_update(GTK_WIDGET(dialog), &playlist_last_dir, NULL);

        g_snprintf(buf, sizeof buf, "Loading playlist %s", filename);
        ui_display_statustext(buf, 1);

        if (m3u_open(filename, playlist_entry_handler, playlist_directive_handler)) {
            /* clear playlist now */
            gtk_list_store_clear(playlist_model);
            /* clear title and path */
            vsid_playlist_free_title();
            vsid_playlist_free_path();

            /* run the parser to populate the playlist */
            if (!m3u_parse()) {
                g_snprintf(buf, sizeof buf, "Error parsing %s.", filename);
                ui_display_statustext(buf, 0);
            }
            /* remember path for the playlist-save dialog */
            vsid_playlist_set_path(filename);
            m3u_close();
        }
        g_free(filename);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
/* }}} */




/* {{{ Playlist saving */

/** \brief  Create content area widget for the 'save-playlist dialog
 *
 * Create GtkGrid with label and text entry to set/edit the playlist title.
 *
 * \return  GtkGrid
 */
static GtkWidget *create_save_content_area(void)
{
    GtkWidget *grid;
    GtkWidget *label;

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_margin_top(grid, 8);
    gtk_widget_set_margin_start(grid, 16);
    gtk_widget_set_margin_end(grid, 16);
    gtk_widget_set_margin_bottom(grid, 8);

    label = gtk_label_new_with_mnemonic("Playlist _title:");
    playlist_title_entry = gtk_entry_new();
    gtk_widget_set_hexpand(label, FALSE);
    gtk_widget_set_hexpand(playlist_title_entry, TRUE);
    gtk_entry_set_text(GTK_ENTRY(playlist_title_entry), vsid_playlist_get_title());

    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), playlist_title_entry, 1, 0, 1, 1);
    gtk_widget_show_all(grid);
    return grid;
}

/** \brief  Callback for the 'save-playlist' dialog
 *
 * Save the current playlist to \a filename.
 *
 * \param[in]   dialog      save-playlist dialog
 * \param[in]   filename    filename or `NULL` to cancel saving
 * \param[in]   data        extra event data (ignored)
 */
static void playlist_save_dialog_callback(GtkDialog *dialog,
                                          gchar *filename,
                                          gpointer data)
{
    if (filename != NULL) {
        GtkTreeIter iter;
        char buf[256];
        time_t t;
        const struct tm *tinfo;
        const char *title;
        char *filename_ext;

        lastdir_update(GTK_WIDGET(dialog), &playlist_last_dir, NULL);

        /* add .m3u extension if missing */
        filename_ext = util_add_extension_const(filename, "m3u");
        g_free(filename);

        /* update playlist title */
        title = gtk_entry_get_text(GTK_ENTRY(playlist_title_entry));
        if (title == NULL || *title == '\0') {
            vsid_playlist_free_title();
        } else {
            vsid_playlist_set_title(title);
            update_title();
        }

        /* try to open playlist file for writing */
        if (!m3u_create(filename_ext)) {
            ui_error("Failed to open '%s' for writing.", filename_ext);
            lib_free(filename_ext);
            gtk_widget_destroy(GTK_WIDGET(dialog));
            return;
        }
        /* m3u code makes a copy of the path, so we can clean up here */
        lib_free(filename_ext);

        /* add empty line */
        if (!m3u_append_newline()) {
            goto save_error;
        }

        /* add timestamp */
        t = time(NULL);
        tinfo = localtime(&t);
        if (tinfo != NULL) {
            strftime(buf, sizeof buf, "Generated on %Y-%m-%dT%H:%M%z", tinfo);
            if (!m3u_append_comment(buf)) {
                goto save_error;
            }
        }

        /* add VICE version */
#ifdef USE_SVN_REVISION
        g_snprintf(buf, sizeof buf,
                   "Generated by VICE (Gtk) %s r%s",
                   VERSION, VICE_SVN_REV_STRING);
#else
        g_snprintf(buf, sizeof buf, "Generated by VICE (Gtk) %s", VERSION);
#endif
        if (!m3u_append_comment(buf)) {
            goto save_error;
        }

        /* add empty line */
        if (!m3u_append_newline()) {
            goto save_error;
        }

        /* add playlist title, if set */
        if (title != NULL && *title != '\0') {
            if (!m3u_set_playlist_title(title)) {
                goto save_error;
            }
        }

        /* add empty line */
        if (!m3u_append_newline()) {
            goto save_error;
        }

        /* finally! iterate playlist items, writing SID file entries */
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playlist_model), &iter)) {
            do {
                const char *fullpath;
                GValue value = G_VALUE_INIT;

                gtk_tree_model_get_value(GTK_TREE_MODEL(playlist_model),
                                         &iter,
                                         COL_FULL_PATH,
                                         &value);
                fullpath = g_value_get_string(&value);
                if (!m3u_append_entry(fullpath)) {
                    goto save_error;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(playlist_model),
                                              &iter));
        }
        m3u_close();
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
    return;

save_error:
    ui_error("I/O error while writing playlist.");
    m3u_close();
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
/* }}} */



/** \brief  Update title of the widget with number of entries in the list
 */
static void update_title(void)
{
    gchar buffer[1024];
    gint rows;

    /* get number of top level items by using NULL as an iter */
    rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(playlist_model), NULL);

    if (playlist_title == NULL) {
        g_snprintf(buffer, sizeof(buffer), "<b>Playlist (%d)</b>", rows);
    } else {
        g_snprintf(buffer, sizeof(buffer),
                   "<b>%s (%d)</b>",
                   vsid_playlist_get_title(), rows);
    }
    gtk_label_set_markup(GTK_LABEL(title_widget), buffer);
}


/*
 * Event handlers
 */

/** \brief  Event handler for the 'destroy' event of the playlist widget
 *
 * \param[in]   widget  playlist widget
 * \param[in]   data    extra event data (unused)
 */
static void on_destroy(GtkWidget *widget, gpointer data)
{
    vsid_playlist_add_dialog_free();
    vsid_playlist_free_title();
    vsid_playlist_free_path();
    lastdir_shutdown(&playlist_last_dir, NULL);
}

/** \brief  Handler for the 'clicked' event of the load-playlist button
 *
 * Show dialog to load a playlist file.
 *
 * \param[in]   widget  button (ignored)
 * \param[in]   data    extra event data (ignored)
 */
static void on_playlist_load_clicked(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog;

    /* if we don't have a previous directory, we use the HVSC base directory */
    set_playlist_dialogs_default_dir();

    /* create dialog and set the initial directory */
    dialog = vice_gtk3_open_file_dialog("Load playlist",
                                        "Playlist files",
                                        file_chooser_pattern_playlist,
                                        NULL,
                                        playlist_load_callback,
                                        NULL);
    lastdir_set(dialog, &playlist_last_dir, NULL);
    gtk_widget_show_all(dialog);
}
/** \brief  Handler for the 'clicked' event of the 'save-playlist' button
 *
 * Show dialog to save the playlist, optionally setting a playlist title.
 *
 * \param[in]   widget  button (ignored)
 * \param[in]   data    extra event data (ignored)
 */
static void on_playlist_save_clicked(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *content;
    gint rows;

    /* don't try to save an empty playlist */
    rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(playlist_model), NULL);
    if (rows < 1) {
        ui_display_statustext("Error: cannot save empty playlist.", 1);
        return;
    }

    /* if we don't have a previous directory, we use the HVSC base directory */
    set_playlist_dialogs_default_dir();
    /* create dialog and set initial directory */
    dialog = vice_gtk3_save_file_dialog("Save playlist file",
                                        vsid_playlist_get_path(),
                                        TRUE,
                                        NULL,
                                        playlist_save_dialog_callback,
                                        NULL);
    lastdir_set(dialog, &playlist_last_dir, NULL);
    /* add content area widget which allows setting playlist title */
    content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content), create_save_content_area());
    gtk_widget_show_all(dialog);
}

/** \brief  Event handler for the 'row-activated' event of the view
 *
 * Triggered by double-clicking on a SID file in the view
 *
 * \param[in,out]   view    the GtkTreeView instance
 * \param[in]       path    the path to the activated row
 * \param[in]       column  the column in the \a view (unused)
 * \param[in]       data    extra event data (unused)
 */
static void on_row_activated(GtkTreeView *view,
                             GtkTreePath *path,
                             GtkTreeViewColumn *column,
                             gpointer data)
{
    GtkTreeIter iter;
    const gchar *filename;
    GValue value = G_VALUE_INIT;

    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(playlist_model), &iter, path)) {
        debug_gtk3("error: failed to get tree iter.");
        return;
    }

    gtk_tree_model_get_value(GTK_TREE_MODEL(playlist_model),
                             &iter,
                             COL_FULL_PATH,
                             &value);
    filename = g_value_get_string(&value);

    if (ui_vsid_window_load_psid(filename) < 0) {
        /* looks like adding files to the playlist already checks the files
         * being added, so this may not be neccesarry */
        char msg[1024];

        g_snprintf(msg, sizeof(msg), "'%s' is not a valid PSID file", filename);
        ui_display_statustext(msg, 10);
    }

    g_value_unset(&value);
}

/** \brief  Event handler for the 'add SID' button
 *
 * \param[in]   widget  button triggering the event (unused)
 * \param[in]   data    extra event data (unused)
 */
static void on_playlist_append_clicked(GtkWidget *widget, gpointer data)
{
    vsid_playlist_add_dialog_exec(add_files_callback);
}

/** \brief  Event handler for the 'remove' button
 *
 * \param[in]   widget  button triggering the event
 * \param[in]   data    extra event data (unused)
 */
static void on_playlist_remove_clicked(GtkWidget *widget, gpointer data)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
    if (gtk_tree_selection_get_selected(selection,
                                        NULL,
                                        &iter)) {
        gtk_list_store_remove(playlist_model, &iter);
    }
}

/** \brief  Event handler for the 'first' button
 *
 * \param[in]   widget  button triggering the event
 * \param[in]   data    extra event data (unused)
 */
static void on_playlist_first_clicked(GtkWidget *widget, gpointer data)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playlist_model), &iter)) {
        const gchar *filename;
        GtkTreeSelection *selection;
        GValue value = G_VALUE_INIT;

        /* get selection object, unselect all rows and select the first row */
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
        gtk_tree_selection_unselect_all(selection);
        gtk_tree_selection_select_iter(selection, &iter);
        /* scroll row into view */
        scroll_to_iter(&iter);
        gtk_tree_model_get_value(GTK_TREE_MODEL(playlist_model),
                                 &iter,
                                 COL_FULL_PATH,
                                 &value);
        filename = g_value_get_string(&value);

        /* TODO: check result */
        ui_vsid_window_load_psid(filename);
        g_value_unset(&value);
    }
}

/** \brief  Event handler for the 'last' button
 *
 * \param[in]   widget  button triggering the event
 * \param[in]   data    extra event data (unused)
 */
static void on_playlist_last_clicked(GtkWidget *widget, gpointer data)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    /* get selection object and deselect all rows */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
    gtk_tree_selection_unselect_all(selection);

    /* There is no gtk_tree_model_get_iter_last(), so: */
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playlist_model), &iter)) {
        GtkTreeIter prev;
        const gchar *filename;
        GValue value = G_VALUE_INIT;

        /* move to last row */
        prev = iter;
        while (gtk_tree_model_iter_next(GTK_TREE_MODEL(playlist_model), &iter)) {
            prev = iter;
        }
        iter = prev;
        gtk_tree_selection_select_iter(selection, &iter);
        /* scroll row into view */
        scroll_to_iter(&iter);

        /* now get the sid filename and load it */
        gtk_tree_model_get_value(GTK_TREE_MODEL(playlist_model),
                                 &iter,
                                 COL_FULL_PATH,
                                 &value);
        filename = g_value_get_string(&value);
        ui_vsid_window_load_psid(filename);

        g_value_unset(&value);
    }
}

/** \brief  Event handler for the 'clear' button
 *
 * \param[in]   widget  button triggering the event
 * \param[in]   data    extra event data (unused)
 */
static void on_playlist_clear_clicked(GtkWidget *widget, gpointer data)
{
    delete_all_rows(NULL, NULL);
}

/** \brief  Event handler for the 'next' button
 *
 * \param[in]   widget  button triggering the event
 * \param[in]   data    extra event data (unused)
 */
static void on_playlist_next_clicked(GtkWidget *widget, gpointer data)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *model;

    /* we can't take the address of GTK_TREE_MODEL(x) so we need this: */
    model = GTK_TREE_MODEL(playlist_model);
    /* get selection object and temporarily set mode to single-selection so
     * we can get an iter (this will keep the 'anchor' selected: the first
     * row clicked when selecting multiple rows) */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    if (gtk_tree_selection_get_selected(selection,
                                        &model,
                                        &iter)) {
        if (gtk_tree_model_iter_next(model, &iter)) {
            const gchar *filename;
            GValue value = G_VALUE_INIT;

            gtk_tree_selection_select_iter(selection, &iter);
            scroll_to_iter(&iter);
            gtk_tree_model_get_value(model,
                                     &iter,
                                     COL_FULL_PATH,
                                     &value);
            filename = g_value_get_string(&value);
            ui_vsid_window_load_psid(filename);
            g_value_unset(&value);
        }
    }
    /* restore multi-select */
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
}

/** \brief  Go to previous entry in the playlist
 *
 * \param[in]   widget  button triggering the event (unused)
 * \param[in]   data    extra event data (unused)
 */
static void on_playlist_prev_clicked(GtkWidget *widget, gpointer data)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *model;

    /* we can't take the address of GTK_TREE_MODEL(x) so we need this: */
    model = GTK_TREE_MODEL(playlist_model);
    /* get selection object and temporarily set mode to single-selection so
     * we can get an iter (this will keep the 'anchor' selected: the first
     * row clicked when selecting multiple rows) */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    if (gtk_tree_selection_get_selected(selection,
                                        &model,
                                        &iter)) {
        if (gtk_tree_model_iter_previous(model, &iter)) {
            const gchar *filename;
            GValue value = G_VALUE_INIT;

            gtk_tree_selection_select_iter(selection, &iter);
            scroll_to_iter(&iter);
            gtk_tree_model_get_value(model,
                                     &iter,
                                     COL_FULL_PATH,
                                     &value);
            filename = g_value_get_string(&value);
            ui_vsid_window_load_psid(filename);
            g_value_unset(&value);
        }
    }
    /* restore multi-select */
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
}


/** \brief  Playlist context menu items
 */
static const ctx_menu_item_t cmenu_items[] = {
    { "Play",
      NULL,
      CTX_MENU_ACTION },
    { "Delete selected item(s)",
      on_ctx_delete_selected_rows,
      CTX_MENU_ACTION },

    { "---", NULL, CTX_MENU_SEP },

    { "Load playlist",
      NULL,
      CTX_MENU_ACTION },
    { "Save playlist",
      NULL,
      CTX_MENU_ACTION },
    { "Clear playlist",
      NULL,
      CTX_MENU_ACTION },

    { "---", NULL, CTX_MENU_SEP },


    { "Export binary",
      NULL,
      CTX_MENU_ACTION },
    { NULL, NULL, -1 }
};


/** \brief  Create context menu for the playlist
 *
 * \return  GtkMenu
 */
static GtkWidget *create_context_menu(void)
{
    GtkWidget *menu;
    GtkWidget *item;
    int i;

    menu = gtk_menu_new();

    for (i = 0; cmenu_items[i].text != NULL; i++) {

        if (cmenu_items[i].type == CTX_MENU_SEP) {
            item = gtk_separator_menu_item_new();
        } else {
            item = gtk_menu_item_new_with_label(cmenu_items[i].text);
            if (cmenu_items[i].callback != NULL) {
                g_signal_connect(
                        item,
                        "activate",
                        G_CALLBACK(cmenu_items[i].callback),
                        GINT_TO_POINTER(i));
            }
        }
        gtk_container_add(GTK_CONTAINER(menu), item);

    }
    gtk_widget_show_all(menu);
    return menu;
}

/** \brief  Event handler for button press events on the playlist
 *
 * Pops up a context menu on the playlist.
 *
 * \param[in]   view    playlist view widget (unused)
 * \param[in]   event   event reference
 * \param[in]   data    extra even data (unused)
 *
 * \return  TRUE when event consumed and no further propagation is needed
 */
static gboolean on_button_press_event(GtkWidget *view,
                                      GdkEvent *event,
                                      gpointer data)
{

    if (((GdkEventButton *)event)->button == GDK_BUTTON_SECONDARY) {
        gtk_menu_popup_at_pointer(GTK_MENU(create_context_menu()), event);
        return TRUE;
    }
    return FALSE;
}


/** \brief  Playlist hotkeys
 */
static const vsid_hotkey_t hotkeys[] = {
    { GDK_KEY_Insert,   0,              open_add_dialog },
    { GDK_KEY_Delete,   0,              on_ctx_delete_selected_rows },
    { GDK_KEY_Delete,   GDK_SHIFT_MASK, delete_all_rows },
    { 0, 0, NULL }
};


/** \brief  Event handler for key press events on the playlist
 *
 * \param[in,out]   view    playlist view widget
 * \param[in]       event   event reference
 * \param[in]       data    extra even data
 *
 * \return  TRUE when event consumed and no further propagation is needed
 */
static gboolean on_key_press_event(GtkWidget *view,
                                   GdkEvent *event,
                                   gpointer data)
{
    GdkEventType type = ((GdkEventKey *)event)->type;

    if (type == GDK_KEY_PRESS) {
        guint keyval = ((GdkEventKey *)event)->keyval;  /* key ID */
        guint modifiers = ((GdkEventKey *)event)->state;    /* modifiers */
        int i;

        for (i = 0; hotkeys[i].keyval != 0; i++) {
            if (hotkeys[i].keyval == keyval
                    && hotkeys[i].modifiers == modifiers
                    && hotkeys[i].callback != NULL) {
                return hotkeys[i].callback(view, event);
            }
        }
    }
    return FALSE;
}

/** \brief  Create playlist model
 */
static void vsid_playlist_model_create(void)
{
    playlist_model = gtk_list_store_new(NUM_COLUMNS,
                                        G_TYPE_STRING,      /* title */
                                        G_TYPE_STRING,      /* author */
                                        G_TYPE_STRING,      /* full path */
                                        G_TYPE_STRING);     /* stripped path */
}

/** \brief  Create playlist view widget
 *
 */
static void vsid_playlist_view_create(void)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    playlist_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(playlist_model));

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            "Title",
            renderer,
            "text", COL_TITLE,
            NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(playlist_view), column);

    column = gtk_tree_view_column_new_with_attributes(
            "Author",
            renderer,
            "text", COL_AUTHOR,
            NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(playlist_view), column);

    column = gtk_tree_view_column_new_with_attributes(
            "Path",
            renderer,
            "text", COL_DISPLAY_PATH,
            NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(playlist_view), column);

    /* Allow selecting multiple items (for deletion) */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    /*
     * Set event handlers
     */

    /* Enter/double-click */
    g_signal_connect(playlist_view,
                     "row-activated",
                     G_CALLBACK(on_row_activated),
                     NULL);
    /* context menu (right-click, or left-click) */
    g_signal_connect(playlist_view,
                     "button-press-event",
                     G_CALLBACK(on_button_press_event),
                     playlist_view);
    /* special keys (Del for now) */
    g_signal_connect(playlist_view,
                     "key-press-event",
                     G_CALLBACK(on_key_press_event),
                     playlist_view);
}


/** \brief  List of playlist controls
 */
static const plist_ctrl_button_t controls[] = {
    { "media-skip-backward",
      BUTTON_PUSH,
      on_playlist_first_clicked,
      "Go to start of playlist" },
    { "media-seek-backward",
      BUTTON_PUSH,
      on_playlist_prev_clicked,
      "Go to previous tune" },
    { "media-seek-forward",
      BUTTON_PUSH,
      on_playlist_next_clicked,
      "Go to next tune" },
    { "media-skip-forward",
      BUTTON_PUSH,
      on_playlist_last_clicked,
      "Go to end of playlist" },
    { "media-playlist-repeat",
      BUTTON_TOGGLE,
      NULL,
      "Repeat playlist" },
    { "media-playlist-shuffle",
      BUTTON_TOGGLE,
      NULL,
      "Shuffle playlist" },
    { "list-add",
      BUTTON_PUSH,
      on_playlist_append_clicked,
      "Add tune to playlist" },
    { "list-remove",
      BUTTON_PUSH,
      on_playlist_remove_clicked,
      "Remove selected tune from playlist" },
    { "document-open",
      BUTTON_PUSH,
      on_playlist_load_clicked,
      "Open playlist file" },
    { "document-save",
      BUTTON_PUSH,
      on_playlist_save_clicked,
      "Save playlist file" },
    { "edit-clear-all",
      BUTTON_PUSH,
      on_playlist_clear_clicked,
      "Clear playlist" },
    { NULL, 0, NULL, NULL }
};

/** \brief  Create a grid with a list of buttons to control the playlist
 *
 * Most of the playlist should also be controllable via a context-menu
 * (ie mouse right-click), which is a big fat TODO now.
 *
 * \return  GtkGrid
 */
static GtkWidget *vsid_playlist_controls_create(void)
{
    GtkWidget *grid;
    int i;

    grid = vice_gtk3_grid_new_spaced(0, VICE_GTK3_DEFAULT);
    for (i = 0; controls[i].icon_name != NULL; i++) {
        GtkWidget *button;
        gchar buff[1024];

        g_snprintf(buff, sizeof(buff), "%s-symbolic", controls[i].icon_name);
        button = gtk_button_new_from_icon_name(buff, GTK_ICON_SIZE_LARGE_TOOLBAR);
        /* always show icon and don't grab focus on click/tab */
        gtk_button_set_always_show_image(GTK_BUTTON(button), TRUE);
        gtk_widget_set_can_focus(button, FALSE);

        gtk_grid_attach(GTK_GRID(grid), button, i, 0, 1, 1);
        if (controls[i].callback != NULL) {
            g_signal_connect(button,
                             "clicked",
                             G_CALLBACK(controls[i].callback),
                             (gpointer)(controls[i].icon_name));
        }
        if (controls[i].tooltip != NULL) {
            gtk_widget_set_tooltip_text(button, controls[i].tooltip);
        }
    }
    return grid;
}


/** \brief  Create main playlist widget
 *
 * \return  GtkGrid
 */
GtkWidget *vsid_playlist_widget_create(void)
{
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *scroll;

    vsid_playlist_model_create();
    vsid_playlist_view_create();

    grid = vice_gtk3_grid_new_spaced(VICE_GTK3_DEFAULT, VICE_GTK3_DEFAULT);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Playlist:</b>");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(label, 8);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    title_widget = label;

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scroll, 600, 180);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), playlist_view);
    gtk_grid_attach(GTK_GRID(grid), scroll, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
                    vsid_playlist_controls_create(),
                    0, 2, 1, 1);

    g_signal_connect_unlocked(grid,
                              "destroy",
                              G_CALLBACK(on_destroy),
                              NULL);

    gtk_widget_show_all(grid);
    return grid;
}


/** \brief  Append \a path to the playlist
 *
 * \param[in]   path    path to SID file
 *
 * \return  TRUE on success, FALSE on failure
 */
gboolean vsid_playlist_widget_append_file(const gchar *path)
{
    GtkTreeIter iter;
    hvsc_psid_t psid;
    char name[HVSC_PSID_TEXT_LEN + 1];
    char author[HVSC_PSID_TEXT_LEN + 1];
    char *name_utf8;
    char *author_utf8;

    /* Attempt to parse sid header for title & composer */
    if (!hvsc_psid_open(path, &psid)) {
        vice_gtk3_message_error("VSID",
                "Failed to parse PSID header of '%s'.",
                path);


         debug_gtk3("Perhaps it's a MUS?");
        /* append MUS to playlist
         *
         * TODO: somehow parse the .mus file for author/tune name,
         *       which is pretty much impossible.
         */
        gtk_list_store_append(playlist_model, &iter);
        gtk_list_store_set(playlist_model,
                           &iter,
                           COL_TITLE, "n/a",
                           COL_AUTHOR, "n/a",
                           COL_FULL_PATH, path,
                           COL_DISPLAY_PATH, path,
                           -1);

    } else {
        gchar *display_path;

        /* get SID name and author */
        memcpy(name, psid.name, HVSC_PSID_TEXT_LEN);
        name[HVSC_PSID_TEXT_LEN] = '\0';
        memcpy(author, psid.author, HVSC_PSID_TEXT_LEN);
        author[HVSC_PSID_TEXT_LEN] = '\0';

        name_utf8 = convert_to_utf8(name);
        author_utf8 = convert_to_utf8(author);
        display_path = strip_hvsc_base(path);

        /* append SID to playlist */
        gtk_list_store_append(playlist_model, &iter);
        gtk_list_store_set(playlist_model,
                           &iter,
                           COL_TITLE, name_utf8,
                           COL_AUTHOR, author_utf8,
                           COL_FULL_PATH, path,
                           COL_DISPLAY_PATH, display_path,
                          -1);

        /* clean up */
        g_free(name_utf8);
        g_free(author_utf8);
        g_free(display_path);
        hvsc_psid_close(&psid);
    }

    update_title();
    return TRUE;
}


/** \brief  Remove SID file at \a row from playlist
 *
 * \param[in]   row     row in playlist
 *
 * FIXME:   unlikely this will be used.
 */
void vsid_playlist_widget_remove_file(int row)
{
    if (row < 0) {
        return;
    }
    /* TODO: actually remove item :) */
}
