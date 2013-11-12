/*
 * supergames.c - Cartridge handling, Super Games cart.
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

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64cartsystem.h"
#undef CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64export.h"
#include "c64mem.h"
#include "cartio.h"
#include "cartridge.h"
#include "monitor.h"
#include "snapshot.h"
#include "supergames.h"
#include "types.h"
#include "util.h"
#include "crt.h"

/*
    "Super Games" ("Commodore Arcade 3 in 1")

    FIXME: the following description is guesswork and probably not correct. 
           Since only one ROM using this cartridge type seems to exist (which 
           works with either possible interpretation) we can only make some 
           vague guesses here. someone needs to look at the hardware and find
           out what it really does.

    - This cart uses 4 16Kb banks mapped in at $8000-$BFFF.
    - assuming the i/o register is reset to 0, the cartridge starts up in bank 0
      and in 16k configuration.

    The control registers is at $DF00, and has the following meaning:

    bit   meaning
    ---   -------
     0    bank bit 0
     1    bank bit 1
     2    mode (0 = 16k config 1 = cartridge disabled)
     3    unused ?
    4-7   unused

    notes:

    - EXROM may actually be either hardwired (always active), or bridged with 
      GAME (both works with the existing ROM). however the later allows to
      properly disable the ROM.
    - the software uses $04, $09, $0c when writing to $df00, which suggests that
      bit 3 is also used somehow. however, not only can it be ignored, also
      using bit2=!GAME and bit3=!EXROM doesnt work. (see the mess below)

*/

static int currbank = 0;
static int currmode = 0;
static BYTE regval = 0;

static void supergames_io2_store(WORD addr, BYTE value)
{
    regval = value;
    currbank = value & 3;
    /* currmode = (value >> 2) & 3; */
    currmode = ((value >> 2) & 1) ^ 1;

    cart_romhbank_set_slotmain(currbank);
    cart_romlbank_set_slotmain(currbank);

    /* printf("value: %02x bank: %d mode: %d\n", value, currbank, currmode); */
#if 0
    if (value & 0x04) {
        /* 8k config */
        cart_set_port_exrom_slotmain(1);
        cart_set_port_game_slotmain(0);
    } else {
        /* 16k config */
        cart_set_port_exrom_slotmain(1);
        cart_set_port_game_slotmain(1);
    }
    if (value == 0x0c) {
        /* cartridge off */
        cart_set_port_exrom_slotmain(0);
        cart_set_port_game_slotmain(0);
    }
#endif
#if 0
    switch (currmode) {
        case 0: /* 00 16k config */
            cart_set_port_exrom_slotmain(1);
            cart_set_port_game_slotmain(1);
            break;
        case 1: /* 01 8k config */ /* first */
            /* ok: 00 10 11 */
            cart_set_port_exrom_slotmain(1);
            cart_set_port_game_slotmain(0);
            break;
        case 2: /* 10 16k config */ /* last (soccer) */
            cart_set_port_exrom_slotmain(1);
            cart_set_port_game_slotmain(1);
            break;
        case 3: /* 11 cartridge off */ /* last (other) */
            /* ok: 00,10 */
            cart_set_port_exrom_slotmain(1);
            cart_set_port_game_slotmain(0);
            break;
    }
#endif
    cart_set_port_exrom_slotmain(currmode);
    cart_set_port_game_slotmain(currmode);

    cart_port_config_changed_slotmain();
}

static BYTE supergames_io2_peek(WORD addr)
{
    return regval;
}

static int supergames_dump(void)
{
    mon_out("Bank: %d (%s)\n", currbank, currmode ? "enabled" : "disabled");
    return 0;
}

/* ---------------------------------------------------------------------*/

static io_source_t supergames_device = {
    CARTRIDGE_NAME_SUPER_GAMES,
    IO_DETACH_CART,
    NULL,
    0xdf00, 0xdfff, 0xff,
    0,
    supergames_io2_store,
    NULL,
    supergames_io2_peek,
    supergames_dump,
    CARTRIDGE_SUPER_GAMES,
    0,
    0
};

static io_source_list_t *supergames_list_item = NULL;

static const c64export_resource_t export_res = {
    CARTRIDGE_NAME_SUPER_GAMES, 1, 1, NULL, &supergames_device, CARTRIDGE_SUPER_GAMES
};

/* ---------------------------------------------------------------------*/

void supergames_config_init(void)
{
    /* cart_config_changed_slotmain(CMODE_16KGAME, CMODE_16KGAME, CMODE_READ); */
    supergames_io2_store(0xdf00, 0);
}

void supergames_config_setup(BYTE *rawcart)
{
    memcpy(&roml_banks[0x0000], &rawcart[0x0000], 0x2000);
    memcpy(&romh_banks[0x0000], &rawcart[0x2000], 0x2000);
    memcpy(&roml_banks[0x2000], &rawcart[0x4000], 0x2000);
    memcpy(&romh_banks[0x2000], &rawcart[0x6000], 0x2000);
    memcpy(&roml_banks[0x4000], &rawcart[0x8000], 0x2000);
    memcpy(&romh_banks[0x4000], &rawcart[0xa000], 0x2000);
    memcpy(&roml_banks[0x6000], &rawcart[0xc000], 0x2000);
    memcpy(&romh_banks[0x6000], &rawcart[0xe000], 0x2000);
    /* cart_config_changed_slotmain(CMODE_16KGAME, CMODE_16KGAME, CMODE_READ); */
    supergames_io2_store(0xdf00, 0);
}

/* ---------------------------------------------------------------------*/
static int supergames_common_attach(void)
{
    if (c64export_add(&export_res) < 0) {
        return -1;
    }
    supergames_list_item = io_source_register(&supergames_device);
    return 0;
}

int supergames_bin_attach(const char *filename, BYTE *rawcart)
{
    if (util_file_load(filename, rawcart, 0x10000, UTIL_FILE_LOAD_SKIP_ADDRESS) < 0) {
        return -1;
    }
    return supergames_common_attach();
}

int supergames_crt_attach(FILE *fd, BYTE *rawcart)
{
    crt_chip_header_t chip;

    while (1) {
        if (crt_read_chip_header(&chip, fd)) {
            break;
        }

        if (chip.start != 0x8000 || chip.size != 0x4000 || chip.bank > 3) {
            return -1;
        }

        if (crt_read_chip(rawcart, chip.bank << 14, &chip, fd)) {
            return -1;
        }
    }
    return supergames_common_attach();
}

void supergames_detach(void)
{
    c64export_remove(&export_res);
    io_source_unregister(supergames_list_item);
    supergames_list_item = NULL;
}

/* ---------------------------------------------------------------------*/

#define CART_DUMP_VER_MAJOR   0
#define CART_DUMP_VER_MINOR   0
#define SNAP_MODULE_NAME  "CARTSUPERGAMES"

int supergames_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, SNAP_MODULE_NAME,
                               CART_DUMP_VER_MAJOR, CART_DUMP_VER_MINOR);
    if (m == NULL) {
        return -1;
    }

    if (0
        || (SMW_B(m, (BYTE)currbank) < 0)
        || (SMW_BA(m, roml_banks, 0x8000) < 0)
        || (SMW_BA(m, romh_banks, 0x8000) < 0)) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);
    return 0;
}

int supergames_snapshot_read_module(snapshot_t *s)
{
    BYTE vmajor, vminor;
    snapshot_module_t *m;

    m = snapshot_module_open(s, SNAP_MODULE_NAME, &vmajor, &vminor);
    if (m == NULL) {
        return -1;
    }

    if ((vmajor != CART_DUMP_VER_MAJOR) || (vminor != CART_DUMP_VER_MINOR)) {
        snapshot_module_close(m);
        return -1;
    }

    if (0
        || (SMR_B_INT(m, &currbank) < 0)
        || (SMR_BA(m, roml_banks, 0x8000) < 0)
        || (SMR_BA(m, romh_banks, 0x8000) < 0)) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);

    return supergames_common_attach();
}
