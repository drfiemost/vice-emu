/*
 * final3.h - Cartridge handling, Final cart.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  groepaz <groepaz@gmx.net>
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

#ifndef VICE_FINAL3_H
#define VICE_FINAL3_H

#include <stdio.h>

#include "types.h"

void final_v3_freeze(void);
void final_v3_config_init(void);
void final_v3_config_setup(uint8_t *rawcart);
int final_v3_bin_attach(const char *filename, uint8_t *rawcart);
int final_v3_crt_attach(FILE *fd, uint8_t *rawcart);
void final_v3_detach(void);

struct snapshot_s;

int final_v3_snapshot_write_module(struct snapshot_s *s);
int final_v3_snapshot_read_module(struct snapshot_s *s);

#endif
