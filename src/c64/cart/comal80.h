/*
 * comal80.h - Cartridge handling, Comal80 cart.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
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

#ifndef VICE_COMAL80_H
#define VICE_COMAL80_H

#include <stdio.h>

#include "types.h"

void comal80_config_init(void);
void comal80_config_setup(uint8_t *rawcart);
int comal80_bin_attach(const char *filename, uint8_t *rawcart);
int comal80_crt_attach(FILE *fd, uint8_t *rawcart, int variant);
void comal80_detach(void);

int comal80_cmdline_options_init(void);
int comal80_resources_init(void);
void comal80_resources_shutdown(void);

struct snapshot_s;

int comal80_snapshot_write_module(struct snapshot_s *s);
int comal80_snapshot_read_module(struct snapshot_s *s);

#endif
