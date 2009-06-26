/*
 * c64cia2.c - Definitions for the second MOS6526 (CIA) chip in the C64
 * ($DD00).
 *
 * Written by
 *  Andr� Fachat <fachat@physik.tu-chemnitz.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
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
 * */


#include "vice.h"

#include <stdio.h>

#include "c64.h"
#include "c64_256k.h"
#include "c64mem.h"
#include "c64iec.h"
#include "c64cia.h"
#include "c64parallel.h"
#include "cia.h"
#include "digimax.h"
#include "iecbus.h"
#include "interrupt.h"
#include "joystick.h"
#include "keyboard.h"
#include "lib.h"
#include "log.h"
#include "maincpu.h"
#include "printer.h"
#include "types.h"
#include "vicii.h"

#ifdef HAVE_RS232
#include "rsuser.h"
#endif


void REGPARM2 cia2_store(WORD addr, BYTE data)
{
    digimax_userport_store(addr, data);
    ciacore_store(machine_context.cia2, addr, data);
}

BYTE REGPARM1 cia2_read(WORD addr)
{
    return ciacore_read(machine_context.cia2, addr);
}

BYTE REGPARM1 cia2_peek(WORD addr)
{
    return ciacore_peek(machine_context.cia2, addr);
}

static void cia_set_int_clk(cia_context_t *cia_context, int value, CLOCK clk)
{
    interrupt_set_nmi(maincpu_int_status, cia_context->int_num, value, clk);
}

static void cia_restore_int(cia_context_t *cia_context, int value)
{
    interrupt_restore_nmi(maincpu_int_status, cia_context->int_num, value);
}

#define MYCIA CIA2

/*************************************************************************
 * I/O
 */

/* Current video bank (0, 1, 2 or 3).  */
static int vbank;


static void do_reset_cia(cia_context_t *cia_context)
{
    printer_userport_write_strobe(1);
    printer_userport_write_data((BYTE)0xff);
#ifdef HAVE_RS232
    rsuser_write_ctrl((BYTE)0xff);
    rsuser_set_tx_bit(1);
#endif

    vbank = 0;
    if (c64_256k_enabled)
        c64_256k_cia_set_vbank(vbank);
    else
        mem_set_vbank(vbank);
}


static void pre_store(void)
{
    vicii_handle_pending_alarms_external_write();
}

static void pre_read(void)
{
    vicii_handle_pending_alarms_external(0);
}

static void pre_peek(void)
{
    vicii_handle_pending_alarms_external(0);
}

static void store_ciapa(cia_context_t *cia_context, CLOCK rclk, BYTE byte)
{
    if (cia_context->old_pa != byte) {
        BYTE tmp;
        int new_vbank;

#ifdef HAVE_RS232
        if (rsuser_enabled && ((cia_context->old_pa ^ byte) & 0x04)) {
            rsuser_set_tx_bit(byte & 4);
        }
#endif
        tmp = ~byte;
        new_vbank = tmp & 3;
        if (new_vbank != vbank) {
            vbank = new_vbank;
            if (c64_256k_enabled)
                c64_256k_cia_set_vbank(new_vbank);
            else
                mem_set_vbank(new_vbank);
        }
        (*iecbus_callback_write)((BYTE)tmp, maincpu_clk);
        printer_userport_write_strobe(tmp & 0x04);
    }
}

static void undump_ciapa(cia_context_t *cia_context, CLOCK rclk, BYTE byte)
{
#ifdef HAVE_RS232
    if (rsuser_enabled) {
        rsuser_set_tx_bit((int)(byte & 4));
    }
#endif
    vbank = (byte ^ 3) & 3;
    if (c64_256k_enabled)
        c64_256k_cia_set_vbank(vbank);
    else
        mem_set_vbank(vbank);
    iecbus_cpu_undump((BYTE)(byte ^ 0xff));
}


static void store_ciapb(cia_context_t *cia_context, CLOCK rclk, BYTE byte)
{
    parallel_cable_cpu_write((BYTE)byte);
#ifdef HAVE_RS232
    rsuser_write_ctrl((BYTE)byte);
#endif
    if (extra_joystick_enable && extra_joystick_type == EXTRA_JOYSTICK_CGA) {
        extra_joystick_cga_store(byte);
    }
}

static void pulse_ciapc(cia_context_t *cia_context, CLOCK rclk)
{
    parallel_cable_cpu_pulse();
    printer_userport_write_data((BYTE)(cia_context->old_pb));
}

/* FIXME! */
static inline void undump_ciapb(cia_context_t *cia_context, CLOCK rclk,
                                BYTE byte)
{
    parallel_cable_cpu_undump((BYTE)byte);
    printer_userport_write_data((BYTE)byte);
#ifdef HAVE_RS232
    rsuser_write_ctrl((BYTE)byte);
#endif
    if (extra_joystick_enable && extra_joystick_type == EXTRA_JOYSTICK_CGA) {
        extra_joystick_cga_store(byte);
    }
}

/* read_* functions must return 0xff if nothing to read!!! */
static BYTE read_ciapa(cia_context_t *cia_context)
{
    BYTE value;

    value = ((cia_context->c_cia[CIA_PRA] | ~(cia_context->c_cia[CIA_DDRA]))
            & 0x3f) | (*iecbus_callback_read)(maincpu_clk);

    if (extra_joystick_enable && extra_joystick_type == EXTRA_JOYSTICK_HIT) {
        value &= 0xfb;
        value |= (joystick_value[3] & 0x10) ? 0 : 4;
    }
    return value;
}

/* read_* functions must return 0xff if nothing to read!!! */
static BYTE read_ciapb(cia_context_t *cia_context)
{
    BYTE byte;
#ifdef HAVE_RS232
    if (rsuser_enabled)
        byte = rsuser_read_ctrl();
    else
#endif
    if (extra_joystick_enable) {
        switch (extra_joystick_type) {
            case EXTRA_JOYSTICK_HIT:
                byte = ~((joystick_value[3] & 0xf) | ((joystick_value[4] & 0xf) << 4));
            case EXTRA_JOYSTICK_CGA:
                byte = extra_joystick_cga_read();
                break;
            case EXTRA_JOYSTICK_PET:
                byte = ((joystick_value[3] & 0xf) | (joystick_value[4] & 0xf) << 4);
                byte |= (joystick_value[3] & 0x10) ? 3 : 0;
                byte |= (joystick_value[4] & 0x10) ? 0x30 : 0;
                byte = ~(byte);
                break;
            case EXTRA_JOYSTICK_OEM:
                byte = ~(joystick_value[3] & 0x1f);
                break;
        }
    } else {
        byte = parallel_cable_cpu_read();
    }

    byte = (byte & ~(cia_context->c_cia[CIA_DDRB]))
           | (cia_context->c_cia[CIA_PRB] & cia_context->c_cia[CIA_DDRB]);
    return byte;
}

static void read_ciaicr(cia_context_t *cia_context)
{
    parallel_cable_cpu_execute();
}

static void read_sdr(cia_context_t *cia_context)
{
    if (extra_joystick_enable && extra_joystick_type == EXTRA_JOYSTICK_HIT) {
        cia_context->c_cia[CIA_SDR] = extra_joystick_hit_read();
    }
}

static void store_sdr(cia_context_t *cia_context, BYTE byte)
{
}

/* Temporary!  */
void cia2_set_flagx(void)
{
    ciacore_set_flag(machine_context.cia2);
}

void cia2_set_sdrx(BYTE received_byte)
{
    ciacore_set_sdr(machine_context.cia2, received_byte);
}

void cia2_init(cia_context_t *cia_context)
{
    ciacore_init(machine_context.cia2, maincpu_alarm_context,
                 maincpu_int_status, maincpu_clk_guard);
}

void cia2_setup_context(machine_context_t *machine_context)
{
    cia_context_t *cia;

    machine_context->cia2 = lib_calloc(1,sizeof(cia_context_t));
    cia = machine_context->cia2;

    cia->prv = NULL;
    cia->context = NULL;

    cia->rmw_flag = &maincpu_rmw_flag;
    cia->clk_ptr = &maincpu_clk;

    cia->todticks = 100000;

    ciacore_setup_context(cia);

    cia->debugFlag = 0;
    cia->irq_line = IK_NMI;
    cia->myname = lib_msprintf("CIA2");

    cia->undump_ciapa = undump_ciapa;
    cia->undump_ciapb = undump_ciapb;
    cia->store_ciapa = store_ciapa;
    cia->store_ciapb = store_ciapb;
    cia->store_sdr = store_sdr;
    cia->read_ciapa = read_ciapa;
    cia->read_ciapb = read_ciapb;
    cia->read_ciaicr = read_ciaicr;
    cia->read_sdr = read_sdr;
    cia->cia_set_int_clk = cia_set_int_clk;
    cia->cia_restore_int = cia_restore_int;
    cia->do_reset_cia = do_reset_cia;
    cia->pulse_ciapc = pulse_ciapc;
    cia->pre_store = pre_store;
    cia->pre_read = pre_read;
    cia->pre_peek = pre_peek;
}

