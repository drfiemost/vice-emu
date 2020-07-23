/** \file   easyflashwidget.c
 * \brief   Widget to control Easy Flash resources
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

/*
 * $VICERES EasyFlashJumper         x64 x64sc xscpu64 x128
 * $VICERES EasyFlashWriteCRT       x64 x64sc xscpu64 x128
 * $VICERES EasyFlashOptimizeCRT    x64 x64sc xscpu64 x128
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

#include "basedialogs.h"
#include "basewidgets.h"
#include "carthelpers.h"
#include "cartridge.h"
#include "debug_gtk3.h"
#include "machine.h"
#include "openfiledialog.h"
#include "resources.h"
#include "savefiledialog.h"
#include "widgethelpers.h"

#include "easyflashwidget.h"



static void on_save_callback(GtkDialog *dialog, char *filename)
{
    if (filename != NULL) {
        if (carthelpers_save_func(CARTRIDGE_EASYFLASH, filename) < 0) {
            vice_gtk3_message_error("VICE core",
                    "Failed to save '%s'", filename);
        }
        g_free(filename);
    }
}





/** \brief  Handler for the "clicked" event of the "Save As" button
 *
 * \param[in]   widget      button
 * \param[in]   user_data   extra event data (unused)
 */
static void on_save_clicked(GtkWidget *widget, gpointer user_data)
{
#if 0
    filename = vice_gtk3_save_file_dialog("Save EasyFlasg image as ...",
            NULL, TRUE, NULL);
#endif
    vice_gtk3_save_file_dialog(NULL,
            "Save Easyflash image as ...",
            NULL, TRUE, NULL, on_save_callback);




}


/** \brief  Handler for the "clicked" event of the "Flush now" button
 *
 * \param[in]   widget      button
 * \param[in]   user_data   extra event data (unused)
 */
static void on_flush_clicked(GtkWidget *widget, gpointer user_data)
{
    debug_gtk3("flushing EF image.");
    if (carthelpers_flush_func(CARTRIDGE_EASYFLASH) < 0) {
        vice_gtk3_message_error("VICE core",
                "Failed to flush the EasyFlash image");
    }
}


/** \brief  Create Easy Flash widget
 *
 * \param[in]   parent  parent widget
 *
 * \return  GtkGrid
 */
GtkWidget *easyflash_widget_create(GtkWidget *parent)
{
    GtkWidget *grid;
    GtkWidget *jumper;
    GtkWidget *write_crt;
    GtkWidget *optimize_crt;
    GtkWidget *save_button;
    GtkWidget *flush_button;

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);

    jumper = vice_gtk3_resource_check_button_new(
            "EasyFlashJumper", "Set Easy Flash jumper");
    write_crt = vice_gtk3_resource_check_button_new(
            "EasyFlashWriteCRT", "Save image when changed");
    optimize_crt = vice_gtk3_resource_check_button_new(
            "EasyFlashOptimizeCRT", "Optimize image when saving");

    gtk_grid_attach(GTK_GRID(grid), jumper, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), write_crt, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), optimize_crt, 0, 2, 1, 1);

    /* Save image as... */
    save_button = gtk_button_new_with_label("Save image as ...");
    gtk_grid_attach(GTK_GRID(grid), save_button, 1, 0, 1, 1);
    g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_clicked),
            NULL);

    gtk_widget_set_sensitive(save_button,
            (gboolean)(carthelpers_can_save_func(CARTRIDGE_EASYFLASH)));

    /* Flush image now */
    flush_button = gtk_button_new_with_label("Flush image now");
    gtk_grid_attach(GTK_GRID(grid), flush_button, 1, 1, 1, 1);
    g_signal_connect(flush_button, "clicked", G_CALLBACK(on_flush_clicked),
            NULL);

    if (carthelpers_can_flush_func(CARTRIDGE_EASYFLASH)) {
        gtk_widget_set_sensitive(flush_button, TRUE);
    } else {
        gtk_widget_set_sensitive(flush_button, FALSE);
    }

    gtk_widget_show_all(grid);
    return grid;
}
