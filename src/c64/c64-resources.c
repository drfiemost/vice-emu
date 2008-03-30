/*
 * c64-resources.c
 *
 * Written by
 *  Andreas Boose <boose@linux.rz.fh-hannover.de>
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
#include <string.h>

#include "c64cart.h"
#include "c64mem.h"
#include "cartridge.h"
#include "resources.h"
#include "reu.h"
#include "utils.h"

/* Name of the character ROM.  */
static char *chargen_rom_name = NULL;

/* Name of the BASIC ROM.  */
static char *basic_rom_name = NULL;

/* Name of the Kernal ROM.  */
static char *kernal_rom_name = NULL;

/* Kernal revision for ROM patcher.  */
char *kernal_revision;

/* Flag: Do we enable the Emulator ID?  */
int emu_id_enabled;

/* Flag: Do we enable the external REU?  */
int reu_enabled;

#ifdef HAVE_RS232
/* Flag: Do we enable the $DE** ACIA RS232 interface emulation?  */
int acia_de_enabled;
#endif

static int set_chargen_rom_name(resource_value_t v, void *param)
{
    const char *name = (const char *)v;

    if (chargen_rom_name != NULL && name != NULL
        && strcmp(name, chargen_rom_name) == 0)
        return 0;

    util_string_set(&chargen_rom_name, name);

    return mem_load_chargen(chargen_rom_name);
}

static int set_kernal_rom_name(resource_value_t v, void *param)
{
    const char *name = (const char *)v;

    if (kernal_rom_name != NULL && name != NULL
        && strcmp(name, kernal_rom_name) == 0)
        return 0;

    util_string_set(&kernal_rom_name, name);

    return mem_load_kernal(kernal_rom_name);
}

static int set_basic_rom_name(resource_value_t v, void *param)
{
    const char *name = (const char *)v;

    if (basic_rom_name != NULL
        && name != NULL
        && strcmp(name, basic_rom_name) == 0)
        return 0;

    util_string_set(&basic_rom_name, name);

    return mem_load_basic(basic_rom_name);
}

static int set_emu_id_enabled(resource_value_t v, void *param)
{
    if (!(int)v) {
        emu_id_enabled = 0;
        return 0;
    } else if (mem_cartridge_type == CARTRIDGE_NONE) {
        emu_id_enabled = 1;
        return 0;
    } else {
        /* Other extensions share the same address space, so they cannot be
           enabled at the same time.  */
        return -1;
    }
}

/*
 * Some cartridges can coexist with the REU.
 * This list might not be complete, but atleast these are known (to me -> nathan)
 * to work. Feel free to add more coexisting cartridges to this list.
 */
static int reu_coexist_cartridge(void)
{
    int result;

    switch (mem_cartridge_type) {
      case (CARTRIDGE_NONE):
      case (CARTRIDGE_SUPER_SNAPSHOT_V5):
      case (CARTRIDGE_EXPERT):
        result = 1;
        break;
      default:
        result = 0;
    }
    return result;
}

/* FIXME: Should initialize the REU when turned on.  */
static int set_reu_enabled(resource_value_t v, void *param)
{
    if (!(int)v) {
        reu_enabled = 0;
        reu_deactivate();
        return 0;
    } else if (reu_coexist_cartridge() == 1) {
        reu_enabled = 1;
        reu_activate();
        return 0;
    } else {
        /* The REU and the IEEE488 interface share the same address space, so
           they cannot be enabled at the same time.  */
        return -1;
    }
}

/* FIXME: Should patch the ROM on-the-fly.  */
static int set_kernal_revision(resource_value_t v, void *param)
{
    const char *rev = (const char *)v;

    util_string_set(&kernal_revision, rev);
    return 0;
}

#ifdef HAVE_RS232

static int set_acia_de_enabled(resource_value_t v, void *param)
{
    acia_de_enabled = (int)v;
    return 0;
}

#endif


static resource_t resources[] = {
    { "ChargenName", RES_STRING, (resource_value_t)"chargen",
      (resource_value_t *) &chargen_rom_name,
      set_chargen_rom_name, NULL },
    { "KernalName", RES_STRING, (resource_value_t)"kernal",
      (resource_value_t *) &kernal_rom_name,
      set_kernal_rom_name, NULL },
    { "BasicName", RES_STRING, (resource_value_t)"basic",
      (resource_value_t *) &basic_rom_name,
      set_basic_rom_name, NULL },
    { "REU", RES_INTEGER, (resource_value_t)0,
      (resource_value_t *) &reu_enabled,
      set_reu_enabled, NULL },
    { "EmuID", RES_INTEGER, (resource_value_t)0,
      (resource_value_t *) &emu_id_enabled,
      set_emu_id_enabled, NULL },
    { "KernalRev", RES_STRING, (resource_value_t)NULL,
      (resource_value_t *) &kernal_revision,
      set_kernal_revision, NULL },
#ifdef HAVE_RS232
    { "AciaDE", RES_INTEGER, (resource_value_t)0,
      (resource_value_t *) &acia_de_enabled,
      set_acia_de_enabled, NULL },
#endif
    { NULL }
};

int c64_init_resources(void)
{
    return resources_register(resources);
}

