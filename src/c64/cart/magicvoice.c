/*
 * magicvoice.c - Speech Cartridge
 *
 * Written by
 *  groepaz <groepaz@gmx.net>
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

#include "archdep.h"
#include "alarm.h"
#include "c64.h"
#include "c64cart.h"
#include "c64cartmem.h"
#include "c64cartsystem.h"
#include "c64export.h"
#include "c64io.h"
#include "c64mem.h"
#include "cartridge.h"
#include "interrupt.h"
#include "lib.h"
#include "log.h"
#include "magicvoice.h"
#include "machine.h"
#include "maincpu.h"
#include "mon_util.h"
#include "resources.h"
#include "sound.h"
#include "t6721.h"
#include "tpi.h"
#include "types.h"
#include "util.h"

/*
    Magic Voice

    U1  CD40105BE (RCA H 432) - 4bit*16 FIFO
    U2  MOS 6525A - Tri Port Interface
        - mapped at io2 (df80-df87)
    U3  8343SEA (LA05-123) - Gate Array (General Instruments)

    U5  T6721A - Voice Synthesizing LSI, Speech Generator, PARCOR Voice Synthesizer
        D0..D3,WR,EOS connected to PB0..PB3,PB4,PA6+PC2 of 6525
        DTRD,DI,phi2 connected to Gate Array

    U6  MOS 251476-01 (8A-06 4341) - 16K ROM

    U7  74LS257 or 74LS222A - Multiplexer with 3-State outputs (4*2 inputs -> 4 outputs)

        used to select wether a12..a15 for MV Cartridge Port comes from
        - the C64 Cartridge Port
        - PB0..PB3 of the 6525

    (note: yes, U4 is missing. these are referring to the schematics by Joachim Nemetz,
           which is like that :))

    ./src/x64 +cart -cartmv Original_MVSM_251476.bin
    ./src/x64 +cart -cartcrt gorf.crt -cartmv Original_MVSM_251476.bin

    say 0 .. N         say a word
    say "word"         say a word
    rate 0 .. 15?      set speed
    voc <addr>         set vocabulary
    rdy                ?

    - Programm starts after Reset at $FFD3
    copies code from $FF36-$FFD2 to $0200-$029C (157 bytes)
    - Programm continues at $021A
    copies $A000-$BFFF from EPROM to RAM at $A000-$BFFF, 8KB
    copies $E000-$FFFF from EPROM to RAM at $E000-$FFFF, 8KB
    copies $AE62-$B461 from RAM to RAM at $C000-$C5FF (Magic Voice Code)
    - Jump to beginning of Magic Voice code at $C000
*/

/* #define MVDEBUG */
/* #define REGDEBUG */

#ifdef MVDEBUG
#define DBG(x) printf x
#else
#define DBG(x)
#endif

#ifdef REGDEBUG
#define DBGREG(x) printf x
#else
#define DBGREG(x)
#endif

static t6721_state *t6721; /* context for the t6721 chip */
static tpi_context_t *tpi_context; /* context for the TPI chip */

#define MV_ROM_SIZE 0x4000
static BYTE mv_rom[MV_ROM_SIZE];

static int mv_game = 1, mv_exrom = 1;
static int mv_romA000_enabled = 1;
static int mv_romE000_enabled = 1;

static int mv_mapped_game = 1, mv_mapped_exrom = 1;

static int mv_game_enabled = 0; /* gamecart at passthrough enabled */

static int mv_enabled = 0; /* cartridge physically enabled */

static void set_int(unsigned int int_num, int value);

int magicvoice_cart_enabled(void)
{
    return mv_enabled;
}

/*****************************************************************************
 FIFO (CD 40105 BE)
*****************************************************************************/

/* IRQ bits according to TPI Port C */
#define NMI_APD         1
#define NMI_EOS         2
#define NMI_DTRD        3

#define FIFO_LEN        (16)
static unsigned int fifo_buffer = 0;
static int readptr, writeptr;
static int datainfifo = 0;
static int fifo_reset = 0;

static int DTRD = 0;

void update_dtrd(int d)
{
    int prev = DTRD;

    if (d) {
        DTRD = 1;
    } else {

        DTRD = 0;

        if (t6721->apd == 0)
        if (t6721->eos == 0)
        if (t6721->playing == 1)
        {
            if (datainfifo < 4) {
                DTRD = 1;
            } else {
                DTRD = 0;
            }
        }
    }
    if (prev != DTRD) {
#if 1
        if (DTRD) {
            DBG(("MV: set NMI DTRD  %d\n", DTRD));
            cart_trigger_nmi();
        } else {
            DBG(("MV: unset NMI DTRD  %d\n", DTRD));
            cartridge_release_freeze();
        }
#endif
        tpicore_set_int(tpi_context, NMI_DTRD, DTRD);
    }
}

/* hooked to callback of t6721 chip */
static BYTE read_data(t6721_state *t6721, unsigned int *bit)
{
    *bit = 0;

    if (datainfifo < 1) {
        update_dtrd(1);
        return 0;
    }

    datainfifo--;
    if (fifo_buffer & (1 << readptr)) {
        *bit = 1;
    }
    readptr++;

    if (readptr == FIFO_LEN) {
        readptr = 0;
    }

    if (datainfifo < 4) {
        update_dtrd(1);
    } else {
        update_dtrd(0);
    }

    return 1;
}

/*
   writes one bit to the FIFO
*/
static BYTE write_bit_to_fifo(BYTE bit)
{
    if (fifo_reset) {
         /* DBG(("SPEECH: first bit %04x %d\n", writeptr, bit)); */
         datainfifo = 0;
    }

    /* if dtrd==0, then run 1 tick, which makes dtrd==1 */
    if (t6721->dtrd) {
        t6721_update_ticks(t6721, 1);
    }

    if (datainfifo >= FIFO_LEN) {
        update_dtrd(0);
        return 1;
    }

    if (bit) {
        bit = 1;
    }

    fifo_buffer &= ~(1 << writeptr);
    fifo_buffer |= (bit << writeptr);
    writeptr++;

    datainfifo++;
    fifo_reset = 0; /* unset FIFO reset condition on first written byte */

    if (writeptr == FIFO_LEN) {
        writeptr = 0;
    }

    t6721_update_ticks(t6721, 2); /* run 2 ticks, which gives the chip time to read 1 bit */

    update_dtrd(0);
    return 0;
}

/*
   writes one nibble to the FIFO
*/
static void write_data_nibble(BYTE nibble)
{
    int i;
    BYTE mask;

/* DBG(("SPEECH: wr byte %04x\n", nibble)); */
/* DBG(("[%x]", nibble)); */
    for (i = 0, mask = 1; i < 4; ++i, mask <<= 1) {
        if (write_bit_to_fifo(nibble & mask)) {
            return;
        }
    }

}

/* hooked to callback of t6721 chip */
static void set_apd(t6721_state *t6721)
{
    if (t6721->apd) {
        fifo_reset = 1; /* set FIFO reset condition */

        /* reset FIFO */
        writeptr = 0;
        readptr = 0;
        datainfifo = 0;

        update_dtrd(0);
    }
    DBG(("MV: set apd:%x\n", t6721->apd));
    tpicore_set_int(tpi_context, NMI_APD, t6721->apd ^ 1);
}

/* hooked to callback of t6721 chip */
static void set_eos(t6721_state *t6721)
{
    DBG(("MV: set eos:%x\n", t6721->eos));
    tpicore_set_int(tpi_context, NMI_EOS, t6721->eos ^ 1);
}

/*****************************************************************************
 Gate Array
 
18 in   t6721 DTRD
20 in   t6721 phi2
 6 in   t6721 APD (reset, will also reset FIFO)

 2 in   FIFO Q0
 3 in   FIFO Q1
 4 in   FIFO Q2
 5 in   FIFO Q3

 1 out  FIFO CI
19 out  t6721 DI

25 in   C64 Cartridge Port IO2
26 in   C64 Cartridge Port ROML
21 in   C64 Cartridge Port ROMH
16 in   C64 Cartridge Port A15
13 in   C64 Cartridge Port A14
15 in   C64 Cartridge Port A13
12 in   C64 Cartridge Port A12
23 in   C64 Cartridge Port phi2

24 out  C64 Cartridge Port GAME

 7 in?  <- 6525 PC6 (CA) (with pullup) (toggles rom on/off ?)
 8 in?  <- 6525 PB5 (with pullup)
 9 in?  <- 6525 PB6 (with pullup)
10 out  chip select for 6525
22 out  chip select for MV ROM

11 out  MV Cartridge Port ROMH
27 out  MV Cartridge Port ROML
17 out  MV Cartridge Port Multiplexer
        (select wether A12..A15 for MV Cart Port comes from C64 Cart Port or PB0..PB3 of the 6525)

exrom - does not go into the GA but due to the way we do the fake mapping it
        goes into the equations here too

*****************************************************************************/
static int ga_pc6;
static int ga_pb5;
static int ga_pb6;

static void ga_memconfig_changed(int mode)
{
int n = 1;
int this;
static int last;

/*
MV: memconfig changed exrom 0 game 1 pc6 (ram/rom?): 0 pb5: 0 pb6: 0 | A000: 1 E000: 1

MV: memconfig changed game 1 exrom 0 pc6: 0 pb5: 1 pb6: 0 | A000: 0 E000: 0
MV: memconfig changed game 1 exrom 1 pc6: 1 pb5: 1 pb6: 0 | A000: 0 E000: 0 16K Game: 1
MV: memconfig changed exrom 1 pc6: 0 pb5: 1 pb6: 0 | game 1 A000: 1 E000: 1 16K Game: 1
*/

    if (
        ((mv_exrom==0) && (ga_pc6 == 0) && (ga_pb5 == 0) && (ga_pb6 == 0))
       ){
        mv_romE000_enabled = 1;
        mv_romA000_enabled = 1;
        mv_game = 1;
    } else if (
        ((mv_exrom==0) && (ga_pc6 == 0) && (ga_pb5 == 1) && (ga_pb6 == 0))
       ){
        /* before running "no cart" */
        mv_romE000_enabled = 0;
        mv_romA000_enabled = 0;
        mv_game = 0;
        mv_game_enabled = 0;
    } else if (
        ((mv_exrom==0) && (ga_pc6 == 0) && (ga_pb5 == 1) && (ga_pb6 == 1))
       ){
        mv_romE000_enabled = 0;
        mv_romA000_enabled = 0;
        mv_game = 1;
        n = 0;
    } else if (
        ((mv_exrom==0) && (ga_pc6 == 1) && (ga_pb5 == 1) && (ga_pb6 == 0))
       ){
        mv_romE000_enabled = 1;
        mv_romA000_enabled = 1;
        mv_game = 1;
    } else if (
        ((mv_exrom==0) && (ga_pc6 == 1) && (ga_pb5 == 1) && (ga_pb6 == 1))
       ){
        mv_romE000_enabled = 1;
        mv_romA000_enabled = 1;
        mv_game = 1;
        n = 0;
    } else if (
        ((mv_exrom==1) && (ga_pc6 == 0) && (ga_pb5 == 0) && (ga_pb6 == 0))
       ){
        mv_romE000_enabled = 1;
        mv_romA000_enabled = 1;
        mv_game = 1;
    } else if (
        ((mv_exrom==1) && (ga_pc6 == 0) && (ga_pb5 == 1) && (ga_pb6 == 0))
       ){
        mv_romE000_enabled = 0;
        mv_romA000_enabled = 0;
        mv_game = 1;
        mv_game_enabled = 1;
    } else if (
        ((mv_exrom==1) && (ga_pc6 == 0) && (ga_pb5 == 1) && (ga_pb6 == 1))
       ){
        mv_romE000_enabled = 1;
        mv_romA000_enabled = 1;
        mv_game = 1;
    } else if ( ((mv_exrom==1) && (ga_pc6 == 1) && (ga_pb5 == 0) && (ga_pb6 == 0)) ){ /* 12 */
        /* NMI with cart ? */
        mv_romE000_enabled = 0;
        mv_romA000_enabled = 0;
        mv_game = 1;
        mv_game_enabled = 0;
    } else if ( ((mv_exrom==1) && (ga_pc6 == 1) && (ga_pb5 == 1) && (ga_pb6 == 0)) ){
        /* before running "16k cart" */
        mv_romE000_enabled = 0;
        mv_romA000_enabled = 0;
        mv_game = 1;
        mv_game_enabled = 1;
    } else if ( ((mv_exrom==1) && (ga_pc6 == 1) && (ga_pb5 == 1) && (ga_pb6 == 1)) ){
        mv_romE000_enabled = 1;
        mv_romA000_enabled = 1;
        mv_game = 1;
    } else {
        mv_romE000_enabled = 1;
        mv_romA000_enabled = 1;
        mv_game = 1;
        n = 2;
    }

    cartridge_config_changed(2, (mv_mapped_game) | ((mv_mapped_exrom) << 1), mode);

    this = (ga_pb6 << 0) |
           (ga_pb5 << 1) |
           (ga_pc6 << 2) |
           (mv_exrom << 3);

    if (last != this)
    {
        if (n == 2) DBG(("-->"));
        if (n) DBG(("MV: memconfig changed (%2d) exrom %d pc6: %d pb5: %d pb6: %d | game %d A000: %d E000: %d 16K Passthrough: %d\n", this, mv_exrom, ga_pc6, ga_pb5, ga_pb6, mv_game,  mv_romA000_enabled, mv_romE000_enabled, mv_game_enabled));
    }

    last = this;
}

/*****************************************************************************
 callbacks for the TPI
*****************************************************************************/

static void set_int(unsigned int int_num, int value)
{
    static int old;
    int isirq;

    isirq = ((tpi_context->c_tpi[TPI_ILR]  ^ 0x0f) & tpi_context->c_tpi[TPI_IMR]) & 0x0f;
    if (old != isirq) {
        if (isirq) {
            DBG(("MV: NMI set NMI  num:%02x val:%02x ILR:%02x IMR:%02x\n", int_num, value, tpi_context->c_tpi[TPI_ILR], tpi_context->c_tpi[TPI_IMR]));
            cart_trigger_nmi();
        } else {
            DBG(("MV: NMI unset NMI  num:%02x val:%02x ILR:%02x IMR:%02x\n", int_num, value, tpi_context->c_tpi[TPI_ILR], tpi_context->c_tpi[TPI_IMR]));
            cartridge_release_freeze();
        }
    }

    old = isirq;
}

static void restore_int(unsigned int int_num, int value)
{
    DBG(("MV: tpi restore int %02x %02x\n", int_num, value));
}

static void reset(tpi_context_t *tpi_context)
{
    DBG(("MV: tpi reset\n"));
}

/*
PA 6 + PC 2: (= /EOS of T6721, End Of Speech)
LOW: End of Speech (LOW for one Frame only, about 10 or 20ms)
HIGH: No voice is synthesized

PA 7 + PC 3: (= DIR of 40105, Data In Ready)
LOW: FIFO is full/busy
HIGH: FIFO is ready to accept data
*/

/*
    Port A (df80)

    PA0..3      OUT: D0..D3 Data -> FIFO -> Gate Array -> T6721 DI (highest Nibble first)
    PA4         OUT: -> FIFO SI Shift in to FIFO (L->H "Pretty Please")
    PA5         IN:  !GAME of the Magic Voice Cartridge Passthrough Port (with pullup)
    PA6         IN:  !EOS <- T6721 (End of Speech)
    PA7         IN:  <- FIFO CO (Data in Ready)
*/

#define MV_GAME_NOCART  1
#define MV_EXROM_NOCART 1

#define MV_GAME_GAMECART  0
#define MV_EXROM_GAMECART 0

static BYTE read_pa(tpi_context_t *tpi_context)
{
    BYTE byte = 0;

    if (cartridge_getid_slotmain() != CARTRIDGE_NONE) {
        /* passthrough */
        byte |= (MV_GAME_GAMECART << 5); /* passthrough !GAME */
    } else {
        byte |= (MV_GAME_NOCART << 5); /* passthrough !GAME */
    }

    byte |= ((t6721->eos ^ 1) << 6); /* !EOS = End of Speech from T6721 */
    byte |= (DTRD << 7); /* DIR = Data in Ready from 40105 */

    DBG(("MV: read pa %02x\n", byte));

    return byte;
}

static void store_pa(tpi_context_t *tpi_context, BYTE byte)
{
/* DBG(("MV: store pa %02x\n", byte)); */
    if (byte & 0x10) {
        /* out: PB3..PB0 go to D3..D0*/
        write_data_nibble(byte & 0x0f); /* write nibble to FIFO */
    }
}

/*
    Port B (df81)

    PB0..3      OUT: D0..D3 Data -> T6721 D0..D3 (highest Nibble first)
    PB4         OUT: !WR -> T6721 WR (write to T6721, L->H "Pretty Please")
    PB5         OUT? -> Gate Array (with pullup)
    PB6         OUT? -> Gate Array (with pullup)
    PB7         IN:  !EXROM <- Exrom of the MV Cartridge Port (with pullup)

    PB5 w=1 system assign
    PB6 w=0 system assign
*/
static void store_pb(tpi_context_t *tpi_context, BYTE byte)
{
/* DBG(("MV: store pp %02x\n", byte)); */
    t6721->wr = (byte >> 4) & 1; /* wr line */
    /* out: PB3..PB0 go to D3..D0*/
    t6721_store(t6721, byte & 0x0f);

    ga_pb5 = (byte >> 5) & 1;
    ga_pb6 = (byte >> 6) & 1;
    ga_memconfig_changed(CMODE_READ);
}

static BYTE read_pb(tpi_context_t *tpi_context)
{
    BYTE byte = 0;

    byte |= t6721_read(t6721) & 0x0f;
#if 0
    byte |= (1 << 5); /* ? pullup */
    byte |= (1 << 6); /* ? pullup */
#endif
    if (cartridge_getid_slotmain() != CARTRIDGE_NONE) {
        /* passthrough */
        byte |= (MV_EXROM_GAMECART << 7); /* passthrough !EXROM */
    } else {
        byte |= (MV_EXROM_NOCART << 7); /* passthrough !EXROM */
    }

    return byte;
}

/*
    Port C

    PC0              unused ?
    PC1              unused ?
    PC2         IN:  !EOS <- T6721 (End of Speech)
    PC3         IN:  <- FIFO CO (Data in Ready)
    PC4              unused ?
    PC5         OUT: !NMI -> Cartridge Port (automatically generated if /EOS or DIR occurs)
    PC6         OUT: (CA) CA ? <-> Gate Array (with pullup) (toggles rom on/off ?)
    PC7         OUT: (CB) !EXROM -> C64 Cartridge Port
*/
static void store_pc(tpi_context_t *tpi_context, BYTE byte)
{
    DBG(("MV: store pc %02x\n", byte));
    if ((byte & 0x20) == 0){
        DBG(("MV: triggered NMI ?\n"));
        /* OUT: !NMI (automatically generated if /EOS or DIR occurs) */
        /* cartridge_trigger_freeze_nmi_only(); */
    } else {
        DBG(("MV: untriggered NMI ?\n"));
    }

    ga_pc6 = (byte >> 6) & 1;
    ga_memconfig_changed(CMODE_READ);
}

static BYTE read_pc(tpi_context_t *tpi_context)
{
    BYTE byte = (0xff & ~(tpi_context->c_tpi)[TPI_DDPC]) | (tpi_context->c_tpi[TPI_PC] & tpi_context->c_tpi[TPI_DDPC]);
#if 0
    static BYTE byte = 0;

    byte |= ((t6721->eos ^ 1) << 2); /* !EOS (End of Speech) */
    byte |= (DTRD << 3); /* DIR (Data in Ready) */
#endif
    DBG(("MV: read pc %02x\n", byte));
    return byte;
}

static void set_ca(tpi_context_t *tpi_context, int a)
{
/* DBG(("MV: set ca %02x\n", a)); */
    ga_pc6 = (a != 0);
    ga_memconfig_changed(CMODE_READ);
}

static void set_cb(tpi_context_t *tpi_context, int a)
{
/* DBG(("MV: set cb %02x\n", a)); */
    mv_exrom = (a == 0);
    ga_memconfig_changed(CMODE_READ);
}

static void undump_pa(tpi_context_t *tpi_context, BYTE byte)
{
    DBG(("MV: undump pa %02x\n", byte));
}

static void undump_pb(tpi_context_t *tpi_context, BYTE byte)
{
    DBG(("MV: undump pb %02x\n", byte));
}

static void undump_pc(tpi_context_t *tpi_context, BYTE byte)
{
    DBG(("MV: undump pc %02x\n", byte));
}

/*****************************************************************************
 I/O Area
*****************************************************************************/

static void REGPARM2 magicvoice_io2_store(WORD addr, BYTE data)
{
    switch (addr & 7) {
        case 5:
            DBGREG(("MV: @:%04x io2 w %04x %02x (IRQ Mask)\n", reg_pc, addr, data));
            break;
        default:
            DBGREG(("MV: @:%04x io2 w %04x %02x\n", reg_pc, addr, data));
            break;
        case 6:
            break;
    }
    tpicore_store(tpi_context, addr & 7, data);
}

static BYTE REGPARM1 magicvoice_io2_read(WORD addr)
{
    BYTE value = 0;
    value = tpicore_read(tpi_context, addr & 7);
    switch (addr & 7) {
        case 5:
            DBGREG(("MV: @:%04x io2 r %04x %02x (IRQ Mask)\n", reg_pc, addr, value));
            break;
        case 7:
#if 0
            value |= 8; /* hack */
#endif
            DBGREG(("MV: @:%04x io2 r %04x %02x (Active IRQs)\n", reg_pc, addr, value));
            break;
        default:
            DBGREG(("MV: @:%04x io2 r %04x %02x\n", reg_pc, addr, value));
            break;
    }
    return value;
}

static BYTE REGPARM1 magicvoice_io2_peek(WORD addr)
{
    return tpicore_peek(tpi_context, addr & 7);
}

static int REGPARM1 magicvoice_io2_dump(void)
{
    mon_out("TPI\n");
    tpicore_dump(tpi_context);
    mon_out("T6721:\n");
    t6721_dump(t6721);
    return 0;
}

/* ---------------------------------------------------------------------*/

static io_source_t magicvoice_io2_device = {
    "Magic Voice",
    IO_DETACH_CART,
    NULL,
    0xdf00, 0xdfff, 0xff,
    1, /* read is always valid */
    magicvoice_io2_store,
    magicvoice_io2_read,
    magicvoice_io2_peek,
    magicvoice_io2_dump,
    CARTRIDGE_MAGIC_VOICE
};

static io_source_list_t *magicvoice_io2_list_item = NULL;

static const c64export_resource_t export_res = {
    "Magic Voice", 1, 1, NULL, &magicvoice_io2_device, CARTRIDGE_MAGIC_VOICE
};

/* ---------------------------------------------------------------------*/
BYTE REGPARM1 magicvoice_roml_read(WORD addr)
{
    if ((mv_game_enabled) && (cartridge_getid_slotmain() != CARTRIDGE_NONE)) {
        /* "passthrough" */
        return roml_banks[(addr & 0x1fff)];
    } else {
        return mem_read_without_ultimax(addr);
    }
}

BYTE REGPARM1 magicvoice_a000_bfff_read(WORD addr)
{
    if ((mv_game_enabled) && (cartridge_getid_slotmain() != CARTRIDGE_NONE)) {
        /* "passthrough" */
        return romh_banks[(addr & 0x1fff)];
    } else {
        if (mv_romA000_enabled) {
            return mv_rom[(addr & 0x1fff)];
        } else {
            return mem_read_without_ultimax(addr);
        }
    }
}

BYTE REGPARM1 magicvoice_romh_read(WORD addr)
{
    if ((mv_game_enabled) && (cartridge_getid_slotmain() != CARTRIDGE_NONE)) {
            /* "passthrough" */
            /* return romh_banks[(addr & 0x1fff)]; */
            return mem_read_without_ultimax(addr);
        } else {
        if (mv_romE000_enabled) {
            return mv_rom[(addr & 0x1fff) + 0x2000];
        } else {
            return mem_read_without_ultimax(addr);
        }
    }
}

/* ---------------------------------------------------------------------*/

char *magicvoice_filename = NULL;

static int set_magicvoice_enabled(int val, void *param)
{
    int stat = 0;
    DBG(("MV: set_enabled: '%s' %d to %d\n", magicvoice_filename, mv_enabled, val));
    if (mv_enabled && !val) {
        if (magicvoice_io2_list_item == NULL) {
            DBG(("MV: BUG: mv_enabled == 1 and magicvoice_io2_list_item == NULL ?!\n"));
        }
        c64export_remove(&export_res);
        c64io_unregister(magicvoice_io2_list_item);
        magicvoice_io2_list_item = NULL;
        DBG(("MV: set_enabled unregistered\n"));
    } else if (!mv_enabled && val) {
        if (magicvoice_filename) {
            if (*magicvoice_filename) {
                if (cartridge_attach_image(CARTRIDGE_MAGIC_VOICE, magicvoice_filename) < 0) {
                    DBG(("MV: set_enabled did not register\n"));
                    stat = -1;
                } else {
                    DBG(("MV: set_enabled registered\n"));
                    if (c64export_add(&export_res) < 0) {
                        stat = -1;
                    } else {
                        c64io_register(&magicvoice_io2_device);
                        stat = 1;
                    }
                }
            }
        }
    }

    mv_enabled = (stat > 0) ? 1 : 0;
    DBG(("MV: set_enabled done: '%s' %d : %d ret %d\n",magicvoice_filename , val, mv_enabled, stat));
    return stat;
}

static int set_magicvoice_filename(const char *name, void *param)
{
    int enabled;

    if (name != NULL && *name != '\0') {
        if (util_check_filename_access(name) < 0) {
            return -1;
        }
    }
    DBG(("MV: set_name: %d '%s'\n",mv_enabled, magicvoice_filename));

    util_string_set(&magicvoice_filename, name);
    resources_get_int("MagicVoiceCartridgeEnabled", &enabled);

    if (set_magicvoice_enabled(enabled, NULL) < 0 ) {
        lib_free (magicvoice_filename);
        magicvoice_filename = NULL;
        DBG(("MV: set_name done: %d '%s'\n",mv_enabled, magicvoice_filename));
        return -1;
    }
    DBG(("MV: set_name done: %d '%s'\n",mv_enabled, magicvoice_filename));
    return 0;
}

static const resource_string_t resources_string[] = {
    { "MagicVoiceImage", "", RES_EVENT_NO, NULL,
      &magicvoice_filename, set_magicvoice_filename, NULL },
    { NULL }
};
static const resource_int_t resources_int[] = {
    { "MagicVoiceCartridgeEnabled", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &mv_enabled, set_magicvoice_enabled, NULL },
    { NULL }
};

int magicvoice_resources_init(void)
{
    if (resources_register_string(resources_string) < 0) {
        return -1;
    }
    return resources_register_int(resources_int);
}

void magicvoice_resources_shutdown(void)
{
    lib_free(magicvoice_filename);
    magicvoice_filename = NULL;
}

/* ---------------------------------------------------------------------*/
void ga_reset(void)
{
    ga_pc6 = 0;
    ga_pb5 = 0;
    ga_pb6 = 0;
}

void magicvoice_setup_context(machine_context_t *machine_context)
{
    DBG(("MV: setup_context\n"));

    /* FIXME: setup TPI context */
    tpi_context = lib_malloc(sizeof(tpi_context_t));

    tpi_context->prv = NULL;

    tpi_context->rmw_flag = &maincpu_rmw_flag;
    tpi_context->clk_ptr = &maincpu_clk;

    tpi_context->myname = lib_msprintf("TPI");

    tpicore_setup_context(tpi_context);

    tpi_context->tpi_int_num = 1; /* FIXME */

    tpi_context->store_pa = store_pa;
    tpi_context->store_pb = store_pb;
    tpi_context->store_pc = store_pc;
    tpi_context->read_pa = read_pa;
    tpi_context->read_pb = read_pb;
    tpi_context->read_pc = read_pc;
    tpi_context->undump_pa = undump_pa;
    tpi_context->undump_pb = undump_pb;
    tpi_context->undump_pc = undump_pc;
    tpi_context->reset = reset;
    tpi_context->set_ca = set_ca;
    tpi_context->set_cb = set_cb;
    tpi_context->set_int = set_int;
    tpi_context->restore_int = restore_int;

    /* init t6721 chip */
    t6721 = lib_malloc(sizeof(t6721_state));
    t6721_reset(t6721);
    t6721->read_data = read_data;
    t6721->set_apd = set_apd;
    t6721->set_eos = set_eos;
}


/* called at reset */
void magicvoice_config_init(void)
{
    DBG(("MV: magicvoice_config_init\n"));

    mv_exrom = 1;
    ga_reset();
    ga_memconfig_changed(CMODE_READ);
}

void magicvoice_config_setup(BYTE *rawcart)
{
    DBG(("MV: magicvoice_config_setup\n"));
    memcpy(mv_rom, rawcart, 0x4000);
}

int magicvoice_bin_attach(const char *filename, BYTE *rawcart)
{
    FILE *fd;

    fd = fopen(filename, MODE_READ);
    if (!fd) {
        return -1;
    }
    if (fread(rawcart, MV_ROM_SIZE, 1, fd) < 1) {
        fclose(fd);
        return -1;
    }
    fclose(fd);

    DBG(("MV: attach\n"));
    /* can't use the resource here, as that would call attach again */
    /* resources_set_int("MagicVoiceCartridgeEnabled", 1); */
    if (c64export_add(&export_res) < 0) {
        return -1;
    }
    c64io_register(&magicvoice_io2_device);
    mv_enabled = 1;
    return 0;
}

void magicvoice_detach(void)
{
    DBG(("MV: detach %d %p\n", mv_enabled, magicvoice_io2_list_item));
    resources_set_int("MagicVoiceCartridgeEnabled", 0);
}

void magicvoice_init(void)
{
    DBG(("MV: init\n"));
    if (mv_enabled) {
        tpi_context->log = log_open(tpi_context->myname);
    }
}

void magicvoice_reset(void)
{
    DBG(("MV: reset\n"));
    if (mv_enabled) {
        mv_game_enabled = 0;
        mv_exrom = 1;
        ga_reset();
        tpicore_reset(tpi_context);
        ga_memconfig_changed(CMODE_READ);
    }
}

/* ---------------------------------------------------------------------*/

/* FIXME: what are these two about anyway ? */
BYTE magicvoice_sound_machine_read(sound_t *psid, WORD addr)
{
    DBG(("MV: magicvoice_sound_machine_read\n"));

    return 0; /* ? */
}

void magicvoice_sound_machine_store(sound_t *psid, WORD addr, BYTE byte)
{
    DBG(("MV: magicvoice_sound_machine_store\n"));
}

/*
    called periodically for every sound fragment that is played
*/
int magicvoice_sound_machine_calculate_samples(sound_t *psid, SWORD *pbuf, int nr, int interleave, int *delta_t)
{
    int i;
    SWORD *buffer;

    if (mv_enabled) {
        buffer = lib_malloc(nr * 2);

        t6721_update_output(t6721, buffer, nr);

        /* mix generated samples to output */
        for (i = 0; i < nr; i++) {
            pbuf[i * interleave] = sound_audio_mix(pbuf[i * interleave], buffer[i]);
        }

        lib_free(buffer);
    }

    return 0; /* ? */
}

void magicvoice_sound_machine_reset(sound_t *psid, CLOCK cpu_clk)
{
    DBG(("MV: magicvoice_sound_machine_reset\n"));
}

int magicvoice_sound_machine_init(sound_t *psid, int speed, int cycles_per_sec)
{
    if (mv_enabled) {
        DBG(("MV: speech_sound_machine_init: speed %d cycles/sec: %d\n", speed, cycles_per_sec));
        t6721_sound_machine_init(t6721, speed, cycles_per_sec);
    }

    return 0; /* ? */
}

void magicvoice_sound_machine_close(sound_t *psid)
{
    DBG(("MV: magicvoice_sound_machine_close\n"));
}
