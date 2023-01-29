/** \file   settings_reu.c
 * \brief   Settings widget to control REU resources
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

/*
 * $VICERES REU             x64 x64sc xscpu64 x128
 * $VICERES REUsize         x64 x64sc xscpu64 x128
 * $VICERES REUfilename     x64 x64sc xscpu64 x128
 * $VICERES REUImageWrite   x64 x64sc xscpu64 x128
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

#include "cartridge.h"
#include "vice_gtk3.h"

#include "settings_reu.h"


/** \brief  List of supported RAM sizes in KiB/MiB
 *
 * REU sizes tend to be specified in MiB when being 1MiB or higher, not KiB.
 */
static const vice_gtk3_radiogroup_entry_t ram_sizes[] = {
    { "128KiB",   128 },
    { "256KiB",   256 },
    { "512KiB",   512 },
    { "1MiB",    1024 },
    { "2MiB",    2048 },
    { "4MiB",    4096 },
    { "8MiB",    8192 },
    { "16MiB",  16384 },
    { NULL,        -1 }
};


/** \brief  Create radio button group to determine REU RAM size
 *
 * \return  GtkGrid
 */
static GtkWidget *create_reu_size_widget(void)
{
    GtkWidget *grid;
    GtkWidget *group;

    grid = vice_gtk3_grid_new_spaced_with_label(8, 0, "REU Size", 1);
    vice_gtk3_grid_set_title_margin(grid, 8);
    group = vice_gtk3_resource_radiogroup_new("REUsize",
                                              ram_sizes,
                                              GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(group, 8);
    gtk_grid_attach(GTK_GRID(grid), group, 0, 1, 1, 1);
    gtk_widget_show_all(grid);
    return grid;
}

/** \brief  Create widget to load/save REU image file
 *
 * \return  GtkGrid
 */
static GtkWidget *create_reu_image_widget(void)
{
    return cart_image_widget_create(NULL,
                                    NULL,
                                    "REUfilename",
                                    "REUImageWrite",
                                    CARTRIDGE_NAME_REU,
                                    CARTRIDGE_REU);
}


/** \brief  Create widget to control RAM Expansion Module resources
 *
 * \param[in]   parent  parent widget (unused)
 *
 * \return  GtkGrid
 */
GtkWidget *settings_reu_widget_create(GtkWidget *parent)
{
    GtkWidget *grid;
    GtkWidget *reu_enable_widget;
    GtkWidget *reu_size;
    GtkWidget *reu_image;

    grid = vice_gtk3_grid_new_spaced(32, 8);

    reu_enable_widget = carthelpers_create_enable_check_button(CARTRIDGE_NAME_REU,
                                                               CARTRIDGE_REU);
    gtk_grid_attach(GTK_GRID(grid), reu_enable_widget, 0, 0, 2, 1);

    reu_size = create_reu_size_widget();
    gtk_widget_set_margin_top(reu_size, 8);
    gtk_grid_attach(GTK_GRID(grid), reu_size, 0, 1, 1, 1);

    reu_image = create_reu_image_widget();
    gtk_widget_set_margin_top(reu_image, 8);
    gtk_grid_attach(GTK_GRID(grid), reu_image, 1, 1, 1, 1);

    gtk_widget_show_all(grid);
    return grid;
}
