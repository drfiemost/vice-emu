/** \file   src/arch/gtk3/widget/romsetwidget.c
 * \brief   GTK3 ROM set widget
 *
 * Written by
 *  Bas Wassink <b.wassink@ziggo.nl>
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
 */

#include "vice.h"

#include <gtk/gtk.h>

#include "debug_gtk3.h"
#include "basedialogs.h"
#include "basewidgets.h"
#include "filechooserhelpers.h"
#include "machine.h"
#include "diskimage.h"
#include "widgethelpers.h"
#include "ui.h"

#include "romsetwidget.h"

#define ROMSET_DEFAULT  "default.vrs"


/** \brief  Machine ROM types
 *
 * This probably needs a lot of updating once I get to the PET. CBM-II etc.
 */
typedef enum rom_type_e {
    ROM_BASIC,      /**< Basic ROM */
    ROM_KERNAL,     /**< Kernal ROM */
    ROM_CHARGEN     /**< Character set ROM */
} rom_type_t;


/** \brief  ROM info object
 */
typedef struct romset_entry_s {
    const char  *resource;  /**< resource name */
    const char  *label;     /**< label */
    void        (*callback)(GtkWidget *, gpointer); /**< optional extra callback
                                                         (currently unused) */
} romset_entry_t;


/** \brief  List of C64 machine ROMs
 */
static const romset_entry_t c64_machine_roms[] = {
    { "KernalName",     "Kernal",   NULL },
    { "BasicName",      "Basic",    NULL },
    { "ChargenName",    "Chargen",  NULL },
    { NULL, NULL, NULL }
};


/** \brief  List of drive ROMs for unsupported machines
 */
static const romset_entry_t unsupported_drive_roms[] = {
    { NULL, NULL, NULL }
};


/** \brief  List of drive ROMs supported by C64/VIC20
 */
static const romset_entry_t c64_vic20_drive_roms[] = {
    { "DosName1540",    "1540",     NULL },
    { "DosName1541",    "1541",     NULL },
    { "DosName1541ii",  "1541-II",  NULL },
    { "DosName1570",    "1570",     NULL },
    { "DosName1571",    "1571",     NULL },
    { "DosName1581",    "1581",     NULL },
    { "DosName2000",    "2000",     NULL },
    { "DosName4000",    "4000",     NULL },
    { "DosName2031",    "2031",     NULL },
    { "DosName2040",    "2040",     NULL },
    { "DosName3040",    "3040",     NULL },
    { "DosName4040",    "4040",     NULL },
    { "DosName1001",    "1001",     NULL },
    { NULL,         NULL,           NULL }
};


/** \brief  List of drive ROMs supported by C128
 */
static const romset_entry_t c128_drive_roms[] = {
    { "DosName1540",    "1540",     NULL },
    { "DosName1541",    "1541",     NULL },
    { "DosName1541ii",  "1541-II",  NULL },
    { "DosName1570",    "1570",     NULL },
    { "DosName1571",    "1571",     NULL },
    { "DosName1571cr",  "1571CR",   NULL },
    { "DosName1581",    "1581",     NULL },
    { "DosName2000",    "2000",     NULL },
    { "DosName4000",    "4000",     NULL },
    { "DosName2031",    "2031",     NULL },
    { "DosName2040",    "2040",     NULL },
    { "DosName3040",    "3040",     NULL },
    { "DosName4040",    "4040",     NULL },
    { "DosName1001",    "1001",     NULL },
    { NULL,         NULL,           NULL }
};


/** \brief  List of drive ROMs supported by PET/CBM-II (5x0 + 6x0/7x0)
 */
static const romset_entry_t pet_cbm2_drive_roms[] = {
    { "DosName2031",    "2031",     NULL },
    { "DosName2040",    "2040",     NULL },
    { "DosName3040",    "3040",     NULL },
    { "DosName4040",    "4040",     NULL },
    { "DosName1001",    "1001",     NULL },
    { NULL,         NULL,           NULL }
};


/** \brief  List of drive ROMs supported by Plus/4
 */
static const romset_entry_t plus4_drive_roms[] = {
    { "DosName1540",    "1540",     NULL },
    { "DosName1541",    "1541",     NULL },
    { "DosName1541ii",  "1541-II",  NULL },
    { "DosName1551",    "1551",     NULL },
    { "DosName1570",    "1570",     NULL },
    { "DosName1571",    "1571",     NULL },
    { "DosName1581",    "1581",     NULL },
    { "DosName2000",    "2000",     NULL },
    { "DosName4000",    "4000",     NULL },
    { NULL,         NULL,           NULL }
};


/** \brief  ROM file name matching patterns
 */
static const char *rom_file_patterns[] = {
    "*.rom", "*.bin", "*.raw", NULL
};

static GtkWidget *layout = NULL;
static GtkWidget *stack = NULL;
static GtkWidget *switcher = NULL;

static GtkWidget *child_machine_roms = NULL;
static GtkWidget *child_drive_roms = NULL;
static GtkWidget *child_rom_archives = NULL;



static void on_default_romset_load_clicked(void)
{
    debug_gtk3("trying to load '%s' ..", ROMSET_DEFAULT);
    if (machine_romset_file_load(ROMSET_DEFAULT) < 0) {
        debug_gtk3("FAILED!\n");
    } else {
        debug_gtk3("OK\n");
    }
}


static int add_stack_switcher(void)
{
    stack = gtk_stack_new();
    switcher = gtk_stack_switcher_new();

    gtk_stack_set_transition_type(GTK_STACK(stack),
            GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 500);
    gtk_stack_set_homogeneous(GTK_STACK(stack), TRUE);
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher),
            GTK_STACK(stack));
    gtk_widget_set_halign(switcher, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(switcher, TRUE);

    /* switcher goes first, otherwise it ends up a the bottom of the widget,
     * which we don't want, although maybe in a few years having the 'tabs'
     * at the bottom suddenly becomes popular, in which case we simply swap
     * the row number of the stack and the switcher :) */
    gtk_grid_attach(GTK_GRID(layout), switcher, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(layout), stack, 0, 1, 1, 1);

    gtk_widget_show(switcher);
    gtk_widget_show(stack);

    return 2;
}


/** \brief  Add a child widget to the stack
 *
 * \param[in]   child   child widget
 * \param[in]   title   displayed title
 * \param[in]   name    ID to use when referencing the stack's children
 *
 */
static void add_stack_child(GtkWidget *child,
                            const gchar *title,
                            const gchar *name)
{
    gtk_stack_add_titled(GTK_STACK(stack), child, title, name);
}


/** \brief  Create push button to load the default ROMs for the current machine
 *
 * \return  GtkButton
 */
static GtkWidget *button_default_romset_load_create(void)
{
    GtkWidget *button = gtk_button_new_with_label("Load default ROMs");
    g_signal_connect(button, "clicked",
            G_CALLBACK(on_default_romset_load_clicked), NULL);
    return button;
}


/** \brief  Create a list of ROM selection widgets from \a roms
 *
 * \param[in]   list of ROMs
 *
 * \return  GtkGrid
 */
static GtkWidget* create_roms_widget(const romset_entry_t *roms)
{
    GtkWidget *grid;
    int row;

    grid = vice_gtk3_grid_new_spaced(VICE_GTK3_DEFAULT, VICE_GTK3_DEFAULT);

    for (row = 0; roms[row].resource != NULL; row++) {
        GtkWidget *label;
        GtkWidget *browser;

        label = gtk_label_new(roms[row].label);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        browser = vice_gtk3_resource_browser_new(roms[row].resource,
                rom_file_patterns, "ROM files", "Select ROM file",
                NULL /* no label, so the labels get aligned properly */,
                NULL);
        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1,  1);
        gtk_grid_attach(GTK_GRID(grid), browser, 1, row, 1, 1);

    }

    gtk_widget_show_all(grid);
    return grid;

}



static GtkWidget *create_c64_roms_widget(void)
{
    GtkWidget *grid;
#if 0
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);

    gtk_grid_attach(GTK_GRID(grid), button_default_romset_load_create(),
            0, 2, 1, 1);

    gtk_widget_show_all(grid);
#endif

    grid = create_roms_widget(c64_machine_roms);

    return grid;
}


/** \brief  Create a stack widget with widgets for each supported drive ROM
 *
 * \return  GtkGrid
 */
static GtkWidget *create_drive_roms_widget(void)
{
    const romset_entry_t *entries;

    switch (machine_class) {
        case VICE_MACHINE_C64:      /* fall through */
        case VICE_MACHINE_C64SC:    /* fall through */
        case VICE_MACHINE_C64DTV:   /* fall through */
        case VICE_MACHINE_SCPU64:   /* fall through */
        case VICE_MACHINE_VIC20:
            entries = c64_vic20_drive_roms;
            break;
        case VICE_MACHINE_C128:
            entries = c128_drive_roms;
            break;
        case VICE_MACHINE_PET:      /* fall through */
        case VICE_MACHINE_CBM5x0:  /* fall through */
        case VICE_MACHINE_CBM6x0:
            entries = pet_cbm2_drive_roms;
            break;
        case VICE_MACHINE_PLUS4:
            entries = plus4_drive_roms;
            break;
        default:
            entries = unsupported_drive_roms;
            break;
    }
    return create_roms_widget(entries);
}


static GtkWidget *create_c64_sets_widget(void)
{
    GtkWidget *grid;

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);

    gtk_grid_attach(GTK_GRID(grid),
            gtk_label_new("Here go the ROM archive handling widgets"),
            0, 2, 1, 1);

    gtk_widget_show_all(grid);
    return grid;
}



/** \brief  Create layout for C64, C64DTV, C128, SCPU64
 *
 * The C64DTV does not have the 'Drive Expansion ROM' widget.
 *
 */
static void create_c64_layout(void)
{
    int row;    /* no idea where I was going with this */

    row = add_stack_switcher();
    child_machine_roms = create_c64_roms_widget();
    child_drive_roms = create_drive_roms_widget();
    child_rom_archives = create_c64_sets_widget();
    add_stack_child(child_machine_roms, "Machine ROMs", "machine");
    add_stack_child(child_drive_roms, "Drive ROMs", "drive");
    add_stack_child(child_rom_archives, "ROM archives", "archive");
}



/** \brief  Create the main ROM settings widget
 *
 * \param[in]   parent  parent widget (ignored)
 *
 * \return  GtkGrid
 */
GtkWidget *romset_widget_create(GtkWidget *parent)
{
    layout = vice_gtk3_grid_new_spaced(VICE_GTK3_DEFAULT, VICE_GTK3_DEFAULT);

/* For now, pretend anything is a C64. This allows checking the drive ROM
 * widgets */
#if 0
    switch (machine_class) {
        case VICE_MACHINE_C64:      /* fall through */
        case VICE_MACHINE_C64SC:    /* fall through */
        case VICE_MACHINE_SCPU64:   /* fall through */
        case VICE_MACHINE_C128:
            create_c64_layout();
            break;
        default:
            debug_gtk3("ROMset stuff not supported (yet) for %s\n", machine_name);
            break;
    }
#endif
    create_c64_layout();

    gtk_widget_show_all(layout);
    return layout;
}

