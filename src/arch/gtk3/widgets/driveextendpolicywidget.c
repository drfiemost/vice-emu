/** \file   src/arch/gtk3/widgets/driveextendpolicywidget.c
 * \brief   Drive 40-track extend policy widget
 *
 * Written by
 *  Bas Wassink <b.wassink@ziggo.nl>
 *
 * Controls the following resource(s):
 *  Drive[8-11]ExtendPolicy
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

#include <gtk/gtk.h>

#include "basewidgets.h"
#include "widgethelpers.h"
#include "debug_gtk3.h"
#include "resources.h"
#include "drive.h"
#include "drive-check.h"
#include "drivewidgethelpers.h"

#include "driveextendpolicywidget.h"


/** \brief  List of (name,id) tuples for the radio buttons
 */
static ui_radiogroup_entry_t policies[] = {
    { "Never extend", 0 },
    { "Ask on extend", 1 },
    { "Extend on access", 2 },
    { NULL, -1 }
};


/** \brief  Unit number
 */
static int unit_number = 8;

static GtkWidget *radio_group = NULL;


/** \brief  Create 40-track extend policy widget
 *
 * \param[in]   unit    drive unit number (8-11)
 *
 * \return  GtkGrid
 */
GtkWidget *drive_extend_policy_widget_create(int unit)
{
    GtkWidget *grid;
    char buffer[256];

    unit_number = 8;

    grid = uihelpers_create_grid_with_label("40-track policy", 1);
    g_snprintf(buffer, 256, "Drive%dExtendImagePolicy", unit);
    radio_group = resource_radiogroup_create(buffer, policies,
            GTK_ORIENTATION_VERTICAL);
    g_object_set(radio_group, "margin-left", 16, NULL);
    gtk_grid_attach(GTK_GRID(grid), radio_group, 0, 1, 1, 1);
    gtk_widget_show_all(grid);
    return grid;
}


/** \brief  Update the widget with data from \a unit
 *
 * \param[in]   unit    drive unit number (8-11)
 */
void drive_extend_policy_widget_update(GtkWidget *widget, int unit)
{
    int policy;
    int drive_type = ui_get_drive_type(unit);

    unit_number = unit;

    resources_get_int_sprintf("Drive%dExtendImagePolicy", &policy, unit);
    resource_radiogroup_update(radio_group, policy);

    /* determine if this widget is valid for the current drive type */
    gtk_widget_set_sensitive(widget,
            drive_check_extend_policy(drive_type));
}
