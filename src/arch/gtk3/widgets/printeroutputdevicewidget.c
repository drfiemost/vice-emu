/** \file   printeroutputdevicewidget.c
 * \brief   Widget to control printer output device settings
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

/*
 * $VICERES Printer4TextDevice  -vsid
 * $VICERES Printer5TextDevice  -vsid
 * $VICERES Printer6TextDevice  -vsid
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vice_gtk3.h"
#include "resources.h"
#include "printer.h"

#include "printeroutputdevicewidget.h"


/** \brief  List of text output devices
 */
static const vice_gtk3_radiogroup_entry_t device_list[] = {
    { "Device 1",  0 },
    { "Device 2",  1 },
    { "Device 3",  2 },
    { NULL,       -1 }
};


/** \brief  Create widget to control the "Printer[4-6]TextDevice resource
 *
 * \param[in]   device number (4-6)
 *
 * \return  GtkGrid
 */
GtkWidget *printer_output_device_widget_create(int device)
{
    GtkWidget *grid;
    GtkWidget *group;

    grid = vice_gtk3_grid_new_spaced_with_label(8, 0, "Output device", 1);
    vice_gtk3_grid_set_title_margin(grid, 8);
    group = vice_gtk3_resource_radiogroup_new_sprintf("Printer%dTextDevice",
                                                      device_list,
                                                      GTK_ORIENTATION_VERTICAL,
                                                      device);
    gtk_grid_attach(GTK_GRID(grid), group, 0, 1, 1, 1);
    gtk_widget_show_all(grid);
    return grid;
}
