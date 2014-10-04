/*
 * mon_register6502.c - The VICE built-in monitor 6502 register functions.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Daniel Sladic <sladic@eecg.toronto.edu>
 *  Ettore Perazzoli <ettore@comm2000.it>
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

/* #define DEBUG_MON_REGS */

#include "vice.h"

#include <stdio.h>
#include <string.h>

#include "lib.h"
#include "log.h"
#include "mon_register.h"
#include "mon_util.h"
#include "montypes.h"
#include "mos6510.h"
#include "uimon.h"

#ifdef DEBUG_MON_REGS
#define DBG(_x_) printf _x_
#else
#define DBG(_x_)
#endif

#define TEST(x) ((x) != 0)

/* TODO: make the other functions here use this table. when done also do the
 *       same with the other CPUs and finally move common code to mon_register.c
 *
 * TODO: also get rid of the ->next member
 */

static mon_reg_list_t mon_reg_list_6510[9] = {
    {      "PC",    e_PC, 16,                      0, 0, &mon_reg_list_6510[1], 0 },
    {       "A",     e_A,  8,                      0, 0, &mon_reg_list_6510[2], 0 },
    {       "X",     e_X,  8,                      0, 0, &mon_reg_list_6510[3], 0 },
    {       "Y",     e_Y,  8,                      0, 0, &mon_reg_list_6510[4], 0 },
    {      "SP",    e_SP,  8,                      0, 0, &mon_reg_list_6510[5], 0 },
    {      "00",      -1,  8, MON_REGISTER_IS_MEMORY, 0, &mon_reg_list_6510[6], 0 },
    {      "01",      -1,  8, MON_REGISTER_IS_MEMORY, 1, &mon_reg_list_6510[7], 0 },
    {      "FL", e_FLAGS,  8,                      0, 0, &mon_reg_list_6510[8], 0 },
    {"NV-BDIZC", e_FLAGS,  8,  MON_REGISTER_IS_FLAGS, 0, NULL, 0 }
};

static mon_reg_list_t mon_reg_list_6502[7] = {
    {      "PC",    e_PC, 16,                      0, 0, &mon_reg_list_6502[1], 0 },
    {       "A",     e_A,  8,                      0, 0, &mon_reg_list_6502[2], 0 },
    {       "X",     e_X,  8,                      0, 0, &mon_reg_list_6502[3], 0 },
    {       "Y",     e_Y,  8,                      0, 0, &mon_reg_list_6502[4], 0 },
    {      "SP",    e_SP,  8,                      0, 0, &mon_reg_list_6502[5], 0 },
    {      "FL", e_FLAGS,  8,                      0, 0, &mon_reg_list_6502[6], 0 },
    {"NV-BDIZC", e_FLAGS,  8,  MON_REGISTER_IS_FLAGS, 0, NULL, 0 }
};

/* TODO: this function is generic, move it into mon_register.c and remove
         mon_register_valid from the monitor_cpu_type_t struct
*/
/* check if register id is valid, returns 1 on valid, 0 on invalid */
static int mon_register_valid(int mem, int reg_id)
{
    mon_reg_list_t *mon_reg_list, *regs;
    int ret = 0;

    DBG(("mon_register_valid mem: %d id: %d\n", mem, reg_id));

    if (monitor_diskspace_dnr(mem) >= 0) {
        if (!check_drive_emu_level_ok(monitor_diskspace_dnr(mem) + 8)) {
            return 0;
        }
    }

    mon_reg_list = regs = mon_register_list_get(mem);

    do {
        if ((!(regs->flags & MON_REGISTER_IS_MEMORY)) && (regs->id == reg_id)) {
            ret = 1;
            break;
        }
        regs = regs->next;
    } while (regs != NULL);

    lib_free(mon_reg_list);

    return ret;
}

static unsigned int mon_register_get_val(int mem, int reg_id)
{
    mos6510_regs_t *reg_ptr;

    DBG(("mon_register_get_val mem: %d id: %d\n", mem, reg_id));

    if (monitor_diskspace_dnr(mem) >= 0) {
        if (!check_drive_emu_level_ok(monitor_diskspace_dnr(mem) + 8)) {
            return 0;
        }
    }

    reg_ptr = mon_interfaces[mem]->cpu_regs;

    DBG(("mon_register_get_val reg ptr: %p\n", reg_ptr));

    switch (reg_id) {
        case e_A:
            return MOS6510_REGS_GET_A(reg_ptr);
        case e_X:
            return MOS6510_REGS_GET_X(reg_ptr);
        case e_Y:
            return MOS6510_REGS_GET_Y(reg_ptr);
        case e_PC:
            return MOS6510_REGS_GET_PC(reg_ptr);
        case e_SP:
            return MOS6510_REGS_GET_SP(reg_ptr);
        case e_FLAGS:
            return MOS6510_REGS_GET_FLAGS(reg_ptr)
                   | MOS6510_REGS_GET_SIGN(reg_ptr)
                   | (MOS6510_REGS_GET_ZERO(reg_ptr) << 1);
        default:
            log_error(LOG_ERR, "Unknown register!");
    }
    return 0;
}

static void mon_register_set_val(int mem, int reg_id, WORD val)
{
    mos6510_regs_t *reg_ptr;


    if (monitor_diskspace_dnr(mem) >= 0) {
        if (!check_drive_emu_level_ok(monitor_diskspace_dnr(mem) + 8)) {
            return;
        }
    }

    reg_ptr = mon_interfaces[mem]->cpu_regs;

    switch (reg_id) {
        case e_A:
            MOS6510_REGS_SET_A(reg_ptr, (BYTE)val);
            break;
        case e_X:
            MOS6510_REGS_SET_X(reg_ptr, (BYTE)val);
            break;
        case e_Y:
            MOS6510_REGS_SET_Y(reg_ptr, (BYTE)val);
            break;
        case e_PC:
            MOS6510_REGS_SET_PC(reg_ptr, val);
            if (monitor_diskspace_dnr(mem) >= 0) {
                mon_interfaces[mem]->set_bank_base(mon_interfaces[mem]->context);
            }
            break;
        case e_SP:
            MOS6510_REGS_SET_SP(reg_ptr, (BYTE)val);
            break;
        case e_FLAGS:
            MOS6510_REGS_SET_STATUS(reg_ptr, (BYTE)val);
            break;
        default:
            log_error(LOG_ERR, "Unknown register!");
            return;
    }
    force_array[mem] = 1;
}

static void mon_register_print(int mem)
{
    mos6510_regs_t *regs;

    if (monitor_diskspace_dnr(mem) >= 0) {
        if (!check_drive_emu_level_ok(monitor_diskspace_dnr(mem) + 8)) {
            return;
        }
    } else if (mem != e_comp_space) {
        log_error(LOG_ERR, "Unknown memory space!");
        return;
    }

    regs = mon_interfaces[mem]->cpu_regs;

    mon_out("  ADDR A  X  Y  SP 00 01 NV-BDIZC ");

    if (mon_interfaces[mem]->get_line_cycle != NULL) {
        mon_out("LIN CYC  STOPWATCH\n");
    } else {
        mon_out(" STOPWATCH\n");
    }

    mon_out(".;%04x %02x %02x %02x %02x %02x %02x %d%d%c%d%d%d%d%d",
            addr_location(mon_register_get_val(mem, e_PC)),
            mon_register_get_val(mem, e_A),
            mon_register_get_val(mem, e_X),
            mon_register_get_val(mem, e_Y),
            mon_register_get_val(mem, e_SP),
            mon_get_mem_val(mem, 0),
            mon_get_mem_val(mem, 1),
            TEST(MOS6510_REGS_GET_SIGN(regs)),
            TEST(MOS6510_REGS_GET_OVERFLOW(regs)),
            '1',
            TEST(MOS6510_REGS_GET_BREAK(regs)),
            TEST(MOS6510_REGS_GET_DECIMAL(regs)),
            TEST(MOS6510_REGS_GET_INTERRUPT(regs)),
            TEST(MOS6510_REGS_GET_ZERO(regs)),
            TEST(MOS6510_REGS_GET_CARRY(regs)));

    if (mon_interfaces[mem]->get_line_cycle != NULL) {
        unsigned int line, cycle;
        int half_cycle;

        mon_interfaces[mem]->get_line_cycle(&line, &cycle, &half_cycle);

        if (half_cycle == -1) {
            mon_out(" %03i %03i", line, cycle);
        } else {
            mon_out(" %03i %03i %i", line, cycle, half_cycle);
        }
    }
    mon_stopwatch_show(" ", "\n");
}

static const char* mon_register_print_ex(int mem)
{
    static char buff[80];
    mos6510_regs_t *regs;

    if (monitor_diskspace_dnr(mem) >= 0) {
        if (!check_drive_emu_level_ok(monitor_diskspace_dnr(mem) + 8)) {
            return "";
        }
    } else if (mem != e_comp_space) {
        log_error(LOG_ERR, "Unknown memory space!");
        return "";
    }

    regs = mon_interfaces[mem]->cpu_regs;

    sprintf(buff, "A:%02X X:%02X Y:%02X SP:%02x %c%c-%c%c%c%c%c",
            mon_register_get_val(mem, e_A),
            mon_register_get_val(mem, e_X),
            mon_register_get_val(mem, e_Y),
            mon_register_get_val(mem, e_SP),
            MOS6510_REGS_GET_SIGN(regs) ? 'N' : '.',
            MOS6510_REGS_GET_OVERFLOW(regs) ? 'V' : '.',
            MOS6510_REGS_GET_BREAK(regs) ? 'B' : '.',
            MOS6510_REGS_GET_DECIMAL(regs) ? 'D' : '.',
            MOS6510_REGS_GET_INTERRUPT(regs) ? 'I' : '.',
            MOS6510_REGS_GET_ZERO(regs) ? 'Z' : '.',
            MOS6510_REGS_GET_CARRY(regs) ? 'C' : '.');

    return buff;
}

static mon_reg_list_t *mon_register_list_get6502(int mem)
{
    mon_reg_list_t *mon_reg_list, *regs;

    DBG(("mon_register_list_get6502 mem: %d\n", mem));

    /* FIXME: This is not elegant. The destinction between 6502/6510
       should not be done by the memory space.  This will change once
       we have completely separated 6502, 6509, 6510 and Z80. */
    if (mem != e_comp_space) {
        mon_reg_list = regs = lib_malloc(sizeof(mon_reg_list_t) * 7);
        memcpy(mon_reg_list, mon_reg_list_6502, sizeof(mon_reg_list_t) * 7);
    } else {
        mon_reg_list = regs = lib_malloc(sizeof(mon_reg_list_t) * 9);
        memcpy(mon_reg_list, mon_reg_list_6510, sizeof(mon_reg_list_t) * 9);
    }

    do {
        if (regs->flags & MON_REGISTER_IS_MEMORY) {
            regs->val = (unsigned int)mon_get_mem_val(mem, regs->extra);
        } else {
            regs->val = (unsigned int)mon_register_get_val(mem, regs->id);
        }
        regs = regs->next;
    } while (regs != NULL);

    return mon_reg_list;
}

#if 0
static void mon_register_list_set6502(mon_reg_list_t *reg_list, int mem)
{
    do {
        if (!strcmp(reg_list->name, "PC")) {
            mon_register_set_val(mem, e_PC, (WORD)(reg_list->val));
        }
        if (!strcmp(reg_list->name, "A")) {
            mon_register_set_val(mem, e_A, (WORD)(reg_list->val));
        }
        if (!strcmp(reg_list->name, "X")) {
            mon_register_set_val(mem, e_X, (WORD)(reg_list->val));
        }
        if (!strcmp(reg_list->name, "Y")) {
            mon_register_set_val(mem, e_Y, (WORD)(reg_list->val));
        }
        if (!strcmp(reg_list->name, "SP")) {
            mon_register_set_val(mem, e_SP, (WORD)(reg_list->val));
        }
        if (!strcmp(reg_list->name, "00")) {
            mon_set_mem_val(mem, 0, (BYTE)(reg_list->val));
        }
        if (!strcmp(reg_list->name, "01")) {
            mon_set_mem_val(mem, 1, (BYTE)(reg_list->val));
        }
        if (!strcmp(reg_list->name, "NV-BDIZC")) {
            mon_register_set_val(mem, e_FLAGS, (WORD)(reg_list->val));
        }

        reg_list = reg_list->next;
    } while (reg_list != NULL);
}
#endif

void mon_register6502_init(monitor_cpu_type_t *monitor_cpu_type)
{
    monitor_cpu_type->mon_register_get_val = mon_register_get_val;
    monitor_cpu_type->mon_register_set_val = mon_register_set_val;
    monitor_cpu_type->mon_register_print = mon_register_print;
    monitor_cpu_type->mon_register_print_ex = mon_register_print_ex;
    monitor_cpu_type->mon_register_list_get = mon_register_list_get6502;
    /* monitor_cpu_type->mon_register_list_set = mon_register_list_set6502; */
    monitor_cpu_type->mon_register_valid = mon_register_valid;
}
