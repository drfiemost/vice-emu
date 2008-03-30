/*
 * drv-mps803.c - MPS803 printer driver.
 *
 * Written by
 *  Thomas Bretz <tbretz@gsi.de>
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
#include <stdlib.h>
#include <string.h>

#include "driver-select.h"
#include "log.h"
#include "output-file.h"
#include "sysfile.h"
#include "types.h"
#include "utils.h"

#define MPS803_ROM_SIZE (7 * 256)
#define MPS803_ROM_FONT_OFFSET 0

#define MAX_COL 480

struct mps_s
{
    BYTE line[MAX_COL][7];
    int  bitcnt;
    int  repeatn;
    int  pos;
    int  tab;
    char tabc[3];
    int  mode;
};
typedef struct mps_s mps_t;

/* We will make this dynamic later.  */
static BYTE charset[256][7];
static mps_t drv_mps803[3];

#define MPS_REVERSE 0x01
#define MPS_CRSRUP  0x02
#define MPS_BITMODE 0x04
#define MPS_DBLWDTH 0x08
#define MPS_REPEAT  0x10
#define MPS_ESC     0x20

/* Logging goes here.  */
static log_t drv803_log = LOG_ERR;

/* ------------------------------------------------------------------------- */
/* MPS803 printer engine. */

static void set_mode(mps_t *mps, int m)
{
    mps->mode |= m;
}

static void del_mode(mps_t *mps, int m)
{
    mps->mode &= ~m;
}

static int is_mode(mps_t *mps, int m)
{
    return mps->mode & m;
}

static int get_charset_bit(mps_t *mps, int nr, unsigned int col,
                           unsigned int row)
{
    int reverse, result;

    reverse = is_mode(mps, MPS_REVERSE);

    result = charset[nr][row] & (1 << (7 - col)) ? !reverse : reverse;

    return result;
}

static void print_cbm_char(mps_t *mps, char c)
{
    unsigned int y, x;

    if (is_mode(mps, MPS_CRSRUP))
        c += 128;

    for (y = 0; y < 7; y++) {
        if (is_mode(mps, MPS_DBLWDTH))
            for (x = 0; x < 6; x++) {
                mps->line[mps->pos + x * 2][y]
                    = get_charset_bit(mps, c, x, y);
                mps->line[mps->pos + x * 2 + 1][y]
                    = get_charset_bit(mps, c, x, y);
            }
        else
            for (x = 0; x < 6; x++)
                mps->line[mps->pos + x][y] = get_charset_bit(mps, c, x, y);
    }

    mps->pos += is_mode(mps, MPS_DBLWDTH) ? 12 : 6;
}

static void write_line(mps_t *mps, unsigned int prnr)
{
    unsigned int x, y;

    for (y = 0; y < 7; y++) {
        for (x = 0; x < MAX_COL; x++)
            output_file_putc(prnr, mps->line[x][y] ? '*' : ' ');

        output_file_putc(prnr, '\n');
    }

    mps->pos=0;
}

static void clear_buffer(mps_t *mps)
{
    unsigned int x, y;

    for (x = 0; x < MAX_COL; x++)
        for (y = 0; y < 7; y++)
            mps->line[x][y] = 0;
}

static void bitmode_off(mps_t *mps)
{
    unsigned int i, x, y;

    for (i = 0; i < mps->repeatn; i++) {
        for (x = 0; x < mps->bitcnt; x++)
            for (y = 0; y < 7; y++)
                mps->line[mps->pos + x][y]
                    = mps->line[mps->pos-mps->bitcnt + x][y];

        mps->pos += mps->bitcnt;
    }
    del_mode(mps, MPS_BITMODE);
}

static void print_bitmask(mps_t *mps, const char c)
{
    unsigned int y;

    for (y = 0; y < 7; y++)
        mps->line[mps->pos][y] = c & (1 << (6 - y)) ? 1 : 0;

    mps->bitcnt++;
    mps->pos++;
}

static void print_char(mps_t *mps, unsigned int prnr, const BYTE c)
{

    if (mps->pos >= MAX_COL) {  /* flush buffer*/
        write_line(mps, prnr);
        clear_buffer(mps);
    }

    if (mps->tab) {     /* decode tab-number*/
        mps->tabc[2 - mps->tab] = c;

        if (mps->tab == 1)
            mps->pos =
                is_mode(mps, MPS_ESC) ?
                mps->tabc[0] << 8 | mps->tabc[1] :
                atoi(mps->tabc) * 6;

        mps->tab--;
        return;
    }

    del_mode(mps, MPS_ESC);

    if (is_mode(mps, MPS_REPEAT)) {
        mps->repeatn = c;
        del_mode(mps, MPS_REPEAT);
        return;
    }

    if (is_mode(mps, MPS_BITMODE) && c & 128) {
        print_bitmask(mps, c);
        return;
    }

    switch (c) {
      case 8:
        set_mode(mps, MPS_BITMODE);
        mps->bitcnt = 0;
        return;

      case 10:  /* LF*/
        write_line(mps, prnr);
        clear_buffer(mps);
        return;

      case 13:  /* CR*/
        mps->pos = 0;
        del_mode(mps, MPS_CRSRUP);
        write_line(mps, prnr);
        clear_buffer(mps);
        return;

        /*
         * By sending the cursor up code [CHR$(145)] to your printer, folowing
         * characters will be printed in cursor up (graphic) mode until either
         * a carriage return or cursor down code [CHR$(17)] is detected.
         *
         * By sending the cursor down code [CHR$(145)] to your printer,
         * following characters will be printed in business mode until either
         * a carriage return or cursor up code [CHR$(145)] is detected.
         *
         * 1. GRAPHIC MODE Code & Front Table, OMITTED
         * When an old number of CHR$(34) is detected in a line, the control
         * codes $00-$1F and $80-$9F will be made visible by printing a
         * reverse character for each of these controls. This will continue
         * until an even number of quotes [CHR$(34)] has been received or until
         * end of this line.
         *
         * 2. BUSINESS MODE Code & Font Table, OMITTED
         * When an old number of CHR$(34) is detected in a line, the control
         * codes $00-$1F and $80-$9F will be made visible by printing a
         * reverse character for each of these controls. This will continue
         * until an even number of quotes [CHR$(34)] has been received or until
         * end of this line.
         */
	
      case 14:  /* EN on*/
        set_mode(mps, MPS_DBLWDTH);
        if (is_mode(mps, MPS_BITMODE))
            bitmode_off(mps);
        return;

      case 15:  /* EN off*/
        del_mode(mps, MPS_DBLWDTH);
        if (is_mode(mps, MPS_BITMODE))
            bitmode_off(mps);
        return;

      case 16:  /* POS*/
        mps->tab = 2; /* 2 chars (digits) following, number of first char*/
        return;

      case 17:   /* crsr dn*/
        del_mode(mps, MPS_CRSRUP);
        return;

      case 18:
        set_mode(mps, MPS_REVERSE);
        return;

      case 26:   /* repeat last chr$(8) c times.*/
        set_mode(mps, MPS_REPEAT);
        mps->repeatn = 0;
        mps->bitcnt  = 0;
        return;

      case 27:
        set_mode(mps, MPS_ESC); /* followed by 16, and number MSB, LSB*/
        return;

      case 145: /* CRSR up*/
        set_mode(mps, MPS_CRSRUP);
        return;

      case 146: /* 18+128*/
        del_mode(mps, MPS_REVERSE);
        return;
    }

    if (is_mode(mps, MPS_BITMODE))
        return;

    print_cbm_char(mps, c);
}

static int init_charset(BYTE *charset, const char *name)
{
    BYTE romimage[MPS803_ROM_SIZE];

    if (sysfile_load(name, romimage, MPS803_ROM_SIZE, MPS803_ROM_SIZE) < 0) {
        log_error(drv803_log, "Could not load %s.", name);
        return -1;
    }

    memcpy(charset, &romimage[MPS803_ROM_FONT_OFFSET], 256 * 7);

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Interface to the upper layer.  */

static int drv_mps803_open(unsigned int prnr, unsigned int secondary)
{
    return output_file_open(prnr);
}

static void drv_mps803_close(unsigned int prnr, unsigned int secondary)
{
    output_file_close(prnr);
}

static int drv_mps803_putc(unsigned int prnr, unsigned int secondary, BYTE b)
{
    print_char(&drv_mps803[prnr], prnr, b);
    return 0;
}

static int drv_mps803_getc(unsigned int prnr, unsigned int secondary, BYTE *b)
{
    return output_file_getc(prnr, b);
}

static int drv_mps803_flush(unsigned int prnr, unsigned int secondary)
{
    return output_file_flush(prnr);
}

int drv_mps803_init_resources(void)
{
    driver_select_t driver_select;

    driver_select.drv_name = "mps803";
    driver_select.drv_open = drv_mps803_open;
    driver_select.drv_close = drv_mps803_close;
    driver_select.drv_putc = drv_mps803_putc;
    driver_select.drv_getc = drv_mps803_getc;
    driver_select.drv_flush = drv_mps803_flush;

    driver_select_register(&driver_select);

    return 0;
}

void drv_mps803_init(void)
{
    drv803_log = log_open("MPS803");

    init_charset((BYTE *)charset, "mps803");
}

