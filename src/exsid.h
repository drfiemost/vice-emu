/*
 * exsid.h
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

#ifndef VICE_EXSID_H
#define VICE_EXSID_H

#ifdef HAVE_EXSID

#include "sid-snapshot.h"
#include "types.h"

extern int exsid_open(void);
extern int exsid_close(void);
extern void exsid_reset(void);
extern int exsid_read(uint16_t addr, int chipno);
extern void exsid_store(uint16_t addr, uint8_t val, int chipno);
extern void exsid_set_machine_parameter(long cycles_per_sec);
extern void exsid_set_device(unsigned int chipno, unsigned int device);

extern int exsid_drv_open(void);
extern int exsid_drv_close(void);
extern void exsid_drv_reset(void);
extern int exsid_drv_read(uint16_t addr, int chipno);
extern void exsid_drv_store(uint16_t addr, uint8_t val, int chipno);
extern int exsid_drv_available(void);
extern void exsid_drv_set_device(unsigned int chipno, unsigned int device);

extern void exsid_state_read(int chipno, struct sid_hs_snapshot_state_s *sid_state);
extern void exsid_state_write(int chipno, struct sid_hs_snapshot_state_s *sid_state);

extern void exsid_drv_state_read(int chipno, struct sid_hs_snapshot_state_s *sid_state);
extern void exsid_drv_state_write(int chipno, struct sid_hs_snapshot_state_s *sid_state);
#endif

extern int exsid_available(void);

#endif
