/** \file   selectdirectorydialog.h
 * \brief   GtkFileChooser wrapper to select/create a directory
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

#ifndef VICE_SELECTDIRECTORYDIALOG_H
#define VICE_SELECTDIRECTORYDIALOG_H

#include "vice.h"
#include <gtk/gtk.h>

#if 0
gchar *vice_gtk3_select_directory_dialog(
        const char *title,
        const char *proposed,
        gboolean allow_create,
        const char *path);
#endif

GtkWidget *vice_gtk3_select_directory_dialog(
        const char *title,
        const char *proposed,
        gboolean allow_create,
        const char *path,
        void (*callback)(GtkDialog *, gchar *, gpointer),
        gpointer param);

#endif
