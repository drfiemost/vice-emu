/*
 * exsid.c - Generic exsid abstraction layer.
 *
 * Written by
 *  Leandro Nini
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

#ifdef HAVE_EXSID

#include <string.h>

#include <exSID.h>

#include "sid-snapshot.h"
#include "exsid.h"
#include "types.h"

#define MAX_EXSID_SID 1


/* buffer containing current register state of SIDs */
static uint8_t sidbuf[0x20];

/* 0 = pal, !0 = ntsc */
static uint8_t sid_ntsc = 0;

static long sid_cycles;

static int exsid_is_open = -1;

/* exSID device */
static void* exsidfd = NULL;

int exsid_open(void)
{
    if (exsid_is_open == -1) {
        exsidfd = exSID_new();
        exsid_is_open = exSID_init(exsidfd);
        memset(sidbuf, 0, sizeof(sidbuf));
    }

    return exsid_is_open;
}

int exsid_close(void)
{
    if (exsid_is_open != -1) {
        exSID_audio_op(exsidfd, XS_AU_MUTE);
        exSID_exit(exsidfd);

        exSID_free(exsidfd);
        exsidfd = NULL;
        exsid_is_open = -1;
    }
    return 0;
}

int exsid_read(uint16_t addr, int chipno)
{
    if (!exsid_is_open && chipno < MAX_EXSID_SID) {
        /* use sidbuf[] for write-only registers */
        if (addr <= 0x18) {
            return sidbuf[addr];
        }
        uint8_t val;
        exSID_clkdread(exsidfd, sid_cycles, addr, &val);
        return val;
    }

    return 0;
}

void exsid_store(uint16_t addr, uint8_t val, int chipno)
{
    if (!exsid_is_open && chipno < MAX_EXSID_SID) {
        /* write to sidbuf[] for write-only registers */
        if (addr <= 0x18) {
            sidbuf[addr] = val;
        }
        exSID_clkdwrite(exsidfd, sid_cycles, addr, val);
    }
}

void exsid_set_machine_parameter(long cycles_per_sec)
{
    if (exsid_is_open != -1) {
        sid_cycles = cycles_per_sec;
        sid_ntsc = (uint8_t)((cycles_per_sec <= 1000000) ? 0 : 1);
        int ret = exSID_clockselect(exsidfd, sid_ntsc ? XS_CL_NTSC : XS_CL_PAL);
        //exsid_drv_set_machine_parameter(cycles_per_sec);
    }
}

int exsid_available(void)
{
    exsid_open();

    if (!exsid_is_open) {
        return 1; //FIXME
    }
    return 0;
}

/* ---------------------------------------------------------------------*/

void exsid_state_read(int chipno, struct sid_hs_snapshot_state_s *sid_state)
{
    int i;

    if (chipno < MAX_EXSID_SID) {
        for (i = 0; i < 32; ++i) {
            sid_state->regs[i] = sidbuf[i];
        }
    }
}

void exsid_state_write(int chipno, struct sid_hs_snapshot_state_s *sid_state)
{
    int i;

    if (chipno < MAX_EXSID_SID) {
        for (i = 0; i < 32; ++i) {
            sidbuf[i] = sid_state->regs[i];
            exSID_clkdwrite(exsidfd, sid_cycles, i, sid_state->regs[i]);
        }
    }
}
#else
int exsid_available(void)
{
    return 0;
}
#endif
