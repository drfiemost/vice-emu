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

#include "alarm.h"
#include "exsid.h"
#include "log.h"
#include "maincpu.h"
#include "resources.h"
#include "sid-snapshot.h"
#include "types.h"

#define MAX_EXSID_SID 1

/* Approx 3 PAL screen updates */
#define EXSID_DELAY_CYCLES 50000

//#include <stdio.h>

/* buffer containing current register state of SIDs */
static uint8_t sidbuf[0x20];

static int exsid_is_open = -1;

static int exsid_model = -1;

/* exSID device */
static void* exsidfd = NULL;

static CLOCK exsid_main_clk;
static CLOCK exsid_alarm_clk;
static alarm_t *exsid_alarm = 0;

static void exsid_alarm_handler(CLOCK offset, void *data);

void exsid_reset(void)
{
    if (!exsid_is_open) {
        exsid_main_clk  = maincpu_clk;
        exsid_alarm_clk = EXSID_DELAY_CYCLES;
        alarm_set(exsid_alarm, EXSID_DELAY_CYCLES);
    }
}

static const char* get_model_string(int model) {
    switch (model) {
        case XS_MD_STD:
            return "exSID USB";
        case XS_MD_PLUS:
            return "exSID+ USB";
        default:
            return "unknown";
    }      
}

int exsid_open(void)
{
    uint16_t version;

    if (exsid_is_open == -1) {
        exsidfd = exSID_new();
        if (!exsidfd) {
            log_error(LOG_DEFAULT, "Error allocating exSID structure.\n");
            return -1;
        }
        exsid_is_open = exSID_init(exsidfd);
        if (exsid_is_open) {
            log_error(LOG_DEFAULT, "exSID init error: %s\n", exSID_error_str(exsidfd));
            exSID_free(exsidfd);
            exsidfd = NULL;
            exsid_is_open = -1;
            return -1;
        }
        memset(sidbuf, 0, sizeof(sidbuf));

        exsid_model = exSID_hwmodel(exsidfd);
        if (exsid_model >= 0) {
            version = exSID_hwversion(exsidfd);
            log_message(LOG_DEFAULT, "exSID model: %s rev %c firmware version %d\n",
                        get_model_string(exsid_model), (char)(version>>8), (version & 0xff));
        }

        exsid_alarm = alarm_new(maincpu_alarm_context, "exsid", exsid_alarm_handler, 0);
    }
    exsid_reset();
    log_message(LOG_DEFAULT, "exSID: opened.");

    return exsid_is_open;
}

int exsid_close(void)
{
    if (!exsid_is_open) {
        exSID_audio_op(exsidfd, XS_AU_MUTE);
        exSID_exit(exsidfd);

        exSID_free(exsidfd);
        exsidfd = NULL;
        exsid_is_open = -1;
        alarm_destroy(exsid_alarm);
        exsid_alarm = 0;

        log_message(LOG_DEFAULT, "exSID: closed.");
    }
    return 0;
}

int exsid_read(uint16_t addr, int chipno)
{
    uint8_t val;

    if (!exsid_is_open && chipno < MAX_EXSID_SID) {

        CLOCK cycles = maincpu_clk - exsid_main_clk - 1;
        exsid_main_clk = maincpu_clk;

        //printf("exsid_read %x (%d)\n", addr, cycles);

        while (cycles > 0xffff) {
            /* delay */
            exSID_delay(exsidfd, 0xffff);
            cycles -= 0xffff;
        }

        /* use sidbuf[] for write-only registers */
        if (addr <= 0x18) {
            exSID_delay(exsidfd, cycles);
            return sidbuf[addr];
        }

        if (exSID_clkdread(exsidfd, cycles, addr, &val) < 0)
            log_error(LOG_DEFAULT, "exsid read error\n");
        return val;
    }

    return 0;
}

void exsid_store(uint16_t addr, uint8_t val, int chipno)
{
    if (!exsid_is_open && chipno < MAX_EXSID_SID) {

        CLOCK cycles = maincpu_clk - exsid_main_clk - 1;
        exsid_main_clk = maincpu_clk;

        //printf("exsid_store %x, %x (%d)\n", addr, val, cycles);

        /* write to sidbuf[] for write-only registers */
        if (addr <= 0x18) {
            sidbuf[addr] = val;
        }
        
        while (cycles > 0xffff) {
            /* delay */
            exSID_delay(exsidfd, 0xffff);
            cycles -= 0xffff;
        }

        if (exSID_clkdwrite(exsidfd, cycles, addr, val) < 0)
            log_error(LOG_DEFAULT, "exsid write error\n");
    }
}

void exsid_set_machine_parameter(long cycles_per_sec)
{
    int sid_model = 0;
    int sid_ntsc;

    if (!exsid_is_open) {
        // FIXME not updated on changes
        if (resources_get_int("SidModel", &sid_model) < 0) {
            log_error(LOG_DEFAULT, "exsid: can't retrieve SidModel\n");
        }

        exSID_chipselect(exsidfd, sid_model == 0 ? XS_CS_CHIP0 : XS_CS_CHIP1);

        // only for exSID+
        if (exsid_model == XS_MD_PLUS) {
            exSID_audio_op(exsidfd, sid_model == 0 ? XS_AU_6581_6581 : XS_AU_8580_8580);
            sid_ntsc = (cycles_per_sec <= 1000000) ? 0 : 1;
            exSID_clockselect(exsidfd, sid_ntsc ? XS_CL_NTSC : XS_CL_PAL);
            exSID_audio_op(exsidfd, XS_AU_UNMUTE);
        }

        //exSID_reset(exsidfd);
        //exSID_clkdwrite(exsidfd, 0, 0x18, 0x0f);    // this will offset the internal clock
    }
}

int exsid_available(void)
{
    exsid_open();

    if (!exsid_is_open) {
        return 1;
    }
    return 0;
}

static void exsid_alarm_handler(CLOCK offset, void *data)
{
    CLOCK cycles = (exsid_alarm_clk + offset) - exsid_main_clk;

    if (cycles < EXSID_DELAY_CYCLES) {
        exsid_alarm_clk = exsid_main_clk + EXSID_DELAY_CYCLES;
    } else {
        int delay = (int) cycles;
        exSID_delay(exsidfd, delay);
        exsid_main_clk   = maincpu_clk - offset;
        exsid_alarm_clk  = exsid_main_clk + EXSID_DELAY_CYCLES;
    }
    alarm_set(exsid_alarm, exsid_alarm_clk);
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
            exSID_clkdwrite(exsidfd, 0, i, sid_state->regs[i]);
        }
    }
}
#else
int exsid_available(void)
{
    return 0;
}
#endif
