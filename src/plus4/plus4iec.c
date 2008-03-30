/*
 * plus4iec.c - IEC bus handling for the Plus4.
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
#include <string.h>

#include "ciad.h"
#include "drive.h"
#include "drivecpu.h"
#include "maincpu.h"
#include "iecdrive.h"
#include "plus4iec.h"
#include "types.h"
#include "via.h"
#include "viad.h"


/* Status of the IEC bus signals.  */
static iec_info_t iec_info;

static BYTE iec_old_atn = 0x10;

int iec_callback_index = 0;

void plus4iec_init(void)
{
    memset(&iec_info, 0xff, sizeof(iec_info_t));
    iec_info.drive_port = 0x85;
}

inline static void iec_update_cpu_bus(BYTE data)
{
    iec_info.cpu_bus = (((data << 7) & 0x80)
                       | ((data << 5) & 0x40)
                       | ((data << 2) & 0x10));
}

inline static void iec_update_ports(void)
{
    iec_info.cpu_port = iec_info.cpu_bus & iec_info.drive_bus
                        & iec_info.drive2_bus;
    iec_info.drive_port = iec_info.drive2_port = (((iec_info.cpu_port >> 4)
                          & 0x4)
                          | (iec_info.cpu_port >> 7)
                          | ((iec_info.cpu_bus << 3) & 0x80));
}

void iec_update_ports_embedded(void)
{
    iec_update_ports();
}

void iec_drive0_write(BYTE data)
{
    iec_info.drive_bus = (((data << 3) & 0x40)
                         | ((data << 6) & ((~data ^ iec_info.cpu_bus) << 3)
                         & 0x80));
    iec_info.drive_data = data;
    iec_update_ports();
}

void iec_drive1_write(BYTE data)
{
    iec_info.drive2_bus = (((data << 3) & 0x40)
                          | ((data << 6) & ((~data ^ iec_info.cpu_bus) << 3)
                          & 0x80));
    iec_info.drive2_data = data;
    iec_update_ports();
}

BYTE iec_drive0_read(void)
{
    return iec_info.drive_port;
}

BYTE iec_drive1_read(void)
{
    return iec_info.drive2_port;
}


/* No drive is enabled.  */
void iec_cpu_write_conf0(BYTE data)
{
    iec_info.iec_fast_1541 = data;
}

/* Only the first drive is enabled.  */
void iec_cpu_write_conf1(BYTE data)
{
    drive0_cpu_execute(maincpu_clk);

    iec_update_cpu_bus(data);

    if (iec_old_atn != (iec_info.cpu_bus & 0x10)) {
        iec_old_atn = iec_info.cpu_bus & 0x10;
        if (drive[0].type != DRIVE_TYPE_1581)
            viacore_signal(&(drive0_context.via1d1541), VIA_SIG_CA1,
                           iec_old_atn ? 0 : VIA_SIG_RISE);
        else
            if (!iec_old_atn)
                ciacore_set_flag(&(drive0_context.cia1581));
    }

    if (drive[0].type != DRIVE_TYPE_1581)
        iec_info.drive_bus = (((iec_info.drive_data << 3) & 0x40)
                             | ((iec_info.drive_data << 6)
                             & ((~iec_info.drive_data ^ iec_info.cpu_bus) << 3)
                             & 0x80));
    else
        iec_info.drive_bus = (((iec_info.drive_data << 3) & 0x40)
                             | ((iec_info.drive_data << 6)
                             & ((iec_info.drive_data | iec_info.cpu_bus) << 3)
                             & 0x80));

    iec_update_ports();
}

/* Only the second drive is enabled.  */
void iec_cpu_write_conf2(BYTE data)
{
    drive1_cpu_execute(maincpu_clk);

    iec_update_cpu_bus(data);

    if (iec_old_atn != (iec_info.cpu_bus & 0x10)) {
        iec_old_atn = iec_info.cpu_bus & 0x10;
        if (drive[1].type != DRIVE_TYPE_1581)
            viacore_signal(&(drive1_context.via1d1541), VIA_SIG_CA1,
                           iec_old_atn ? 0 : VIA_SIG_RISE);
        else
            if (!iec_old_atn)
                ciacore_set_flag(&(drive1_context.cia1581));
    }

    if (drive[1].type != DRIVE_TYPE_1581)
        iec_info.drive2_bus = (((iec_info.drive2_data << 3) & 0x40)
                              | ((iec_info.drive2_data << 6)
                              & ((~iec_info.drive2_data ^ iec_info.cpu_bus) << 3)
                              & 0x80));
    else
        iec_info.drive2_bus = (((iec_info.drive2_data << 3) & 0x40)
                              | ((iec_info.drive2_data << 6)
                              & ((iec_info.drive2_data | iec_info.cpu_bus) << 3)
                              & 0x80));

    iec_update_ports();
}

/* Both drive are enabled.  */
void iec_cpu_write_conf3(BYTE data)
{
    drive0_cpu_execute(maincpu_clk);
    drive1_cpu_execute(maincpu_clk);

    iec_update_cpu_bus(data);

    if (iec_old_atn != (iec_info.cpu_bus & 0x10)) {
        iec_old_atn = iec_info.cpu_bus & 0x10;

        if (drive[0].type != DRIVE_TYPE_1581)
            viacore_signal(&(drive0_context.via1d1541), VIA_SIG_CA1,
                           iec_old_atn ? 0 : VIA_SIG_RISE);
        else
            if (!iec_old_atn)
                ciacore_set_flag(&(drive0_context.cia1581));

        if (drive[1].type != DRIVE_TYPE_1581)
            viacore_signal(&(drive1_context.via1d1541), VIA_SIG_CA1,
                           iec_old_atn ? 0 : VIA_SIG_RISE);
        else
            if (!iec_old_atn)
                ciacore_set_flag(&(drive1_context.cia1581));

    }

    if (drive[0].type != DRIVE_TYPE_1581)
        iec_info.drive_bus = (((iec_info.drive_data << 3) & 0x40)
                             | ((iec_info.drive_data << 6)
                             & ((~iec_info.drive_data ^ iec_info.cpu_bus) << 3)
                             & 0x80));
    else
        iec_info.drive_bus = (((iec_info.drive_data << 3) & 0x40)
                             | ((iec_info.drive_data << 6)
                             & ((iec_info.drive_data | iec_info.cpu_bus) << 3)
                             & 0x80));

    if (drive[1].type != DRIVE_TYPE_1581)
        iec_info.drive2_bus = (((iec_info.drive2_data << 3) & 0x40)
                              | ((iec_info.drive2_data << 6)
                              & ((~iec_info.drive2_data ^ iec_info.cpu_bus) << 3)
                              & 0x80));
    else
        iec_info.drive2_bus = (((iec_info.drive2_data << 3) & 0x40)
                              | ((iec_info.drive2_data << 6)
                              & ((iec_info.drive2_data | iec_info.cpu_bus) << 3)
                              & 0x80));

    iec_update_ports();
}

void iec_cpu_undump(BYTE data)
{
    iec_update_cpu_bus(data);
    iec_old_atn = iec_info.cpu_bus & 0x10;
}

BYTE iec_cpu_read(void)
{
    if (!drive[0].enable && !drive[1].enable)
	return (iec_info.iec_fast_1541 & 0x30) << 2;

    if (drive[0].enable)
	drive0_cpu_execute(maincpu_clk);
    if (drive[1].enable)
	drive1_cpu_execute(maincpu_clk);
    return iec_info.cpu_port;
}

iec_info_t *iec_get_drive_port(void)
{
    return &iec_info;
}

void parallel_cable_drive0_write(BYTE data, int handshake)
{
}

void parallel_cable_drive1_write(BYTE data, int handshake)
{
}

BYTE parallel_cable_drive_read(int handshake)
{
    return 0;
}

/* This function is called from ui_update_menus() */
int iec_available_busses(void)
{
    return IEC_BUS_IEC;
}

void iec_calculate_callback_index(void)
{
    iec_callback_index = (drive[0].enable ? 1 : 0)
                         | (drive[1].enable ? 2 : 0);
}

void iec_fast_drive_write(BYTE data, unsigned int dnr)
{
/* The Plus4 does not use fast IEC.  */
}

void iec_fast_drive_direction(int direction, unsigned int dnr)
{
}

