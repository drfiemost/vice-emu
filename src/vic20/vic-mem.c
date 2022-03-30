/*
 * vic-mem.c - Memory interface for the VIC-I emulation.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Matthies <andreas.matthies@gmx.net>
 *  Daniel Kahlin <daniel@kahlin.net>
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

#include "joyport.h"
#include "maincpu.h"
#include "raster-changes.h"
#include "types.h"
#include "vic.h"
#include "victypes.h"
#include "vic-mem.h"
#include "vic20.h"
#include "vic20mem.h"
#include "vic20sound.h"
#include "viewport.h"

#ifdef HAVE_MOUSE
#include "lightpen.h"
#include "mouse.h"

static CLOCK pot_cycle = 0;  /* pot sampling cycle */
#endif

/* VIC access functions. */

void vic_store(uint16_t addr, uint8_t value)
{
    addr &= 0xf;
    vic.regs[addr] = value;
    VIC_DEBUG_REGISTER (("VIC: write $90%02X, value = $%02X.", addr, value));

    switch (addr) {
#if 0
        /* handled in vic_cycle.c */
        case 0:                   /* $9000  Screen X Location. */
        /*
            VIC checks in cycle n for peek($9000)=n
            and in this case opens the horizontal flipflop
        */
        case 1:                   /* $9001  Screen Y Location. */
        /*
            VIC checks from cycle 1 of line r*2 to cycle 0 of line r*2+2
            if peek($9001)=r is true and in this case it opens the vertical
            flipflop
        */
        case 2:                   /* $9002  Columns Displayed. */
        case 5:                   /* $9005  Video and char matrix base
                                   address. */
        /* read-only registers */
        case 4:                   /* $9004  Raster line count -- read only. */
        case 6:                   /* $9006. */
        case 7:                   /* $9007  Light Pen X,Y. */
        case 8:                   /* $9008. */
        case 9:                   /* $9009  Paddle X,Y. */
#endif
        default:
            return;

        case 3:                   /* $9003  Rows Displayed, Character size . */
            {
                int new_char_height = (value & 0x1) ? 16 : 8;

                vic.row_increase_line = new_char_height;
                vic.char_height = new_char_height;
            }
            return;

        case 10:                  /* $900A  Bass Enable and Frequency. */
        case 11:                  /* $900B  Alto Enable and Frequency. */
        case 12:                  /* $900C  Soprano Enable and Frequency. */
        case 13:                  /* $900D  Noise Enable and Frequency. */
            vic_sound_store(addr, value);
            return;

        case 14:                  /* $900E  Auxiliary Colour, Master Volume. */
            /*
                changes of auxiliary color in cycle n is visible at pixel 4*(n-7)+1
            */
            {
                static int old_aux_color = -1;
                int new_aux_color = value >> 4;

                if (new_aux_color != old_aux_color) {
                    /* integer part */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR_INT(vic.raster_cycle + 1),
                                                      &vic.auxiliary_color,
                                                      new_aux_color);
                    /* fractional part (half chars) */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR_INT(vic.raster_cycle + 1),
                                                      &vic.half_char_flag,
                                                      VIC_RASTER_CHAR_FRAC(vic.raster_cycle + 1));
                    /* old_mc_auxilary_color is used by vic-draw.c to handle the
                       one hires vic pixel lateness of change */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR(vic.raster_cycle + 2),
                                                      &vic.old_auxiliary_color,
                                                      new_aux_color);

                    old_aux_color = new_aux_color;
                }
            }

            vic_sound_store(addr, value);
            return;

        case 15:                  /* $900F  Screen and Border Colors,
                                     Reverse Video. */
            /*
                changes of border/background in cycle n are visible at pixel
                4*(n-7)+1, changes of reverse mode at pixel 4*(n-7)+3.
            */
            {
                static int old_background_color = -1;
                static int old_border_color = -1;
                static int old_reverse = -1;
                int new_background_color, new_border_color, new_reverse;

                new_background_color = value >> 4;
                new_border_color = value & 0x7;
                new_reverse = ((value & 0x8) ? 0 : 1);

                if (new_background_color != old_background_color) {
                    raster_changes_background_add_int(&vic.raster,
                                                      VIC_RASTER_X(vic.raster_cycle + 1) + VIC_PIXEL_WIDTH,
                                                      (int*)&vic.raster.background_color,
                                                      new_background_color);

                    old_background_color = new_background_color;
                }

                if (new_border_color != old_border_color) {
                    raster_changes_border_add_int(&vic.raster,
                                                  VIC_RASTER_X(vic.raster_cycle + 1) + VIC_PIXEL_WIDTH,
                                                  (int*)&vic.raster.border_color,
                                                  new_border_color);

                    /* we also need the border color in multicolor mode,
                       so we duplicate it */
                    /* integer part */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR_INT(vic.raster_cycle + 1),
                                                      &vic.mc_border_color,
                                                      new_border_color);
                    /* fractional part (half chars) */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR_INT(vic.raster_cycle + 1),
                                                      &vic.half_char_flag,
                                                      VIC_RASTER_CHAR_FRAC(vic.raster_cycle + 1));
                }

                if (new_reverse != old_reverse) {
                    /* integer part */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR_INT(vic.raster_cycle + 1),
                                                      &vic.reverse,
                                                      new_reverse);
                    /* fractional part (half chars) */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR_INT(vic.raster_cycle + 1),
                                                      &vic.half_char_flag,
                                                      VIC_RASTER_CHAR_FRAC(vic.raster_cycle + 1));
                }


                if (new_border_color != old_border_color) {
                    /* old_mc_border_color is used by vic-draw.c to handle the
                       one hires vic pixel lateness of change */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR(vic.raster_cycle + 2),
                                                      &vic.old_mc_border_color,
                                                      new_border_color);

                    old_border_color = new_border_color;
                }

                if (new_reverse != old_reverse) {
                    /* old_reverse is used by vic-draw.c to handle the
                       3 hires vic pixels lateness of change */
                    raster_changes_foreground_add_int(&vic.raster,
                                                      VIC_RASTER_CHAR(vic.raster_cycle + 2),
                                                      &vic.old_reverse,
                                                      new_reverse);

                    old_reverse = new_reverse;
                }

                return;
            }
    }
}

static inline unsigned vic_read_rasterline(void)
{
    unsigned ypos = VIC_RASTER_Y(maincpu_clk + vic.cycle_offset);

    /* TODO: examine/verify what exactly happens, and in what cycles. the over-
     *       all frame timing must be correct (as hardcoded timer values in
     *       games work) so the only way it makes sense is that when rasterline 0
     *       is "shorter", the last rasterline has to be "longer".
     */
    if (vic.cycles_per_line == VIC20_NTSC_CYCLES_PER_LINE) {
        if (vic.interlace_enabled) {
            /* FIXME: unfortunately using this for the VIC_RASTER_Y macro breaks non interlaced
                      timing and perhaps also PAL - we need to investigate this further */
            ypos = ((unsigned int)(((maincpu_clk + vic.cycle_offset) - vic.framestart_cycle) / 65));
            /* interlaced
               top + bottom field combined cycle count: 32+262x65 + 262x65+33 = 525x65 cycles
             */
            if (vic.interlace_field == 0) {
                /*
                 * top
                 * line 0 with 32 cycles
                   lines 1..262 with 65 cycles
                */
                /* HACK: the cycle offset pushes ypos to last+1, this is part of
                         line 0 of the next field */
                if (ypos >= (VIC20_NTSC_INTERLACE_FIELD1_LAST_LINE + 1)) {
                    return 0;
                }
            } else {
                /*
                 * bottom
                 * lines 0..261 with 65 cycles
                   line 262 with 33 cycles
                */
                /* HACK: the last line of this field and the first line of the other field add
                         up to a full line */
                if (ypos >= VIC20_NTSC_INTERLACE_FIELD2_LAST_LINE) {
                    if (VIC_RASTER_CYCLE(maincpu_clk + vic.cycle_offset) < VIC20_NTSC_INTERLACE_FIELD2_CYCLES_LAST_LINE) {
                        return VIC20_NTSC_INTERLACE_FIELD2_LAST_LINE; /* confirm this */
                    } else {
                        return 0;
                    }
                }
            }
            return ypos;
        } else {
            /* no interlace */
            /* line 0 with 32 cycles
               lines 1..260 with 65 cycles
               line 261 with 33 cycles
               total number: 32+260x65+33 = 261x65 cycles
               HACK: we fake this by returning line=261 for the first 33 cycles of line 0
            */
            if (ypos == 0) {
                if (VIC_RASTER_CYCLE(maincpu_clk + vic.cycle_offset) < VIC20_NTSC_CYCLES_LAST_LINE) {
                    return VIC20_NTSC_LAST_LINE; /* confirm this */
                }
            }
        }
    }

    return ypos;
}

uint8_t vic_read(uint16_t addr)
{
    addr &= 0xf;

#ifdef HAVE_MOUSE
    if ((addr == 8) || (addr == 9)) {
        if (_mouse_enabled) {
            if ((maincpu_clk ^ pot_cycle) & ~511) {
                pot_cycle = maincpu_clk & ~511; /* simplistic 512 cycle sampling */

                if (_mouse_enabled) {
                    mouse_poll();
                }

                vic.regs[8] = read_joyport_potx();
                vic.regs[9] = read_joyport_poty();
            }
        }
    }
#endif

    switch (addr) {
        case 3:
            return ((vic_read_rasterline() & 1) << 7) | (vic.regs[3] & ~0x80);
        case 4:
            return vic_read_rasterline() >> 1;
        case 6:
            return vic.light_pen.x;
        case 7:
            return vic.light_pen.y;
        default:
            return vic.regs[addr];
    }
}

uint8_t vic_peek(uint16_t addr)
{
    /* No side effects (unless mouse_get_* counts) */
    return vic_read(addr);
}
