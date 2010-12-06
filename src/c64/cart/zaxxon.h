/*
 * zaxxon.h - Cartridge handling, Zaxxon cart.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
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

#ifndef VICE_ZAXXON_H
#define VICE_ZAXXON_H

#include <stdio.h>

#include "types.h"

extern BYTE REGPARM1 zaxxon_roml_read(WORD addr);

extern void zaxxon_config_init(void);
extern void zaxxon_config_setup(BYTE *rawcart);
extern int zaxxon_bin_attach(const char *filename, BYTE *rawcart);
extern int zaxxon_crt_attach(FILE *fd, BYTE *rawcart);
extern void zaxxon_detach(void);

struct snapshot_s;

extern int zaxxon_snapshot_write_module(struct snapshot_s *s);
extern int zaxxon_snapshot_read_module(struct snapshot_s *s);

#endif
