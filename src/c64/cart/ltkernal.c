/*
 * ltkernal.c - Cartridge handling, Lt. Kernal Host Adapter
 *
 * Written by
 *  Roberto Muscedere <rmusced@uwindsor.ca>
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

#define CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64cartsystem.h"
#undef CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64cart.h"
#include "c64mem.h"
#include "c64cartmem.h"
#include "c64pla.h"
#include "c64-generic.h"
#include "cartio.h"
#include "cartridge.h"
#include "export.h"
#include "ltkernal.h"
#include "maincpu.h"
#include "monitor.h"
#include "snapshot.h"
#include "types.h"
#include "util.h"
#include "crt.h"
#include "lib.h"
#include "machine.h"
#include "ram.h"
#include "resources.h"
#include "cmdline.h"
#include "maincpu.h"
#include "log.h"
#include "mc6821core.h"
#include "scsi.h"
#include "c64memrom.h"
#include "alarm.h"

/* #define LTKLOG1 */
/* #define LTKLOG2 */
/* #define LTKDEBUGIO */
/* #define LTKDEBUGMEM */

#define LOG LOG_DEFAULT

#ifdef LTKDEBUGMEM
#define MDBG(_x_) log_message _x_
#else
#define MDBG(_x_)
#endif

#ifdef LTKDEBUGIO
#define IDBG(_x_) log_message _x_
#else
#define IDBG(_x_)
#endif

#ifdef LTKLOG1
#define LOG1(_x_) log_message _x_
#else
#define LOG1(_x_)
#endif

#ifdef LTKLOG2
#define LOG2(_x_) log_message _x_
#else
#define LOG2(_x_)
#endif

#define CRIT(_x_) log_error _x_

#if C64CART_ROM_LIMIT <= 16384
#error C64CART_ROM_LIMIT is too small; it should be at least 16384.
#endif

/*

Lt. Kernal

*** Brief IO explanation ***

Write $3C to $Dx03 Enables stock kernal (sets CB2 high)
Write $34 to $Dx03 Enables upper RAM as kernal (sets CB2 low)

Write $40 (bit 6) to $Dx02 enables writes to kernal and lower RAM to $8000-$9fff
 only when the upper ram kernal is enabled
Clear $40 (bit 6) of $Dx02 disable writes to kernal and disables lower RAM

If bit 6 of $Dx02 is 0 when bit 3 of $Dx03 is set, the system reset timer
 is started and will assert in 57 cycles
 Setting bit 6 of $Dx02 or clearing vit 2 of $Dx03 will cancel it

Setting bit 6 of $Dx02 to 1 when $Dx03 bit 3 is low turns off ROM

*** Detailed explanation ***

    The Lt. Kernal Host Adaptor (SCSI ID 7) looks on sector 0 of drive 0 (ID
0) to find the partition (LU or logical unit) information and DOS. The DOS
(official latest 7.2) is limited by the original cylinder/head/sector (CHS)
addressing schemes of legacy hard disks, but it doesn't use that when
accessing them; it uses the modern logical block address, although this may
have not been the case in the original version. The CHS is used when
configuring its filing system for each LU. You should never have a disk
configured with more than 15 heads or have the product of the heads and
sectors that exceeds 255 as it will cause data corruption. The DOS also cannot
access an LU which starts beyond cylinder 1023. The solution to this, at the
time, was to use more than one disk. This emulation supports up to 7 drives.
The Lt. Kernal sold in 20 MB and 40 MB versions, so these issues were never
addressed officially. The DOS install disks are keyed to the host adaptors and
hard disks (see LTKserial), so any additions or modifications were done by the
company, Xetec, and sent to the customer on new install disks. Unofficially,
to use a larger disk, the information on track 18, sector 18 of the first
install disk needs to be modified; LTKEDIT2 is a tool which does this. Be
warned it does not check for invalid information. Some of the configurations
LTKEDIT2 offers will cause data corruption as the CHS combinations are too
high. An easy and quick way to check this is to VALIDATE a new LU after
ACTIVATING it.

    The Lt. Kernal Host Adaptor uses the following registers:

    $DF00    = MC6821 Port A: Data and DDR
    $DF01    = MC6821 Port A: Control
    $DF02    = MC6821 Port B: Data and DDR
    $DF03    = MC6821 Port B: Control
    $DF04..7 = LTK Port and Freeze state

MC6821 Port A connects to the SCSI data bus (inverted). Data is saved to hard
disk inverted to save data processing time. The adaptor doesn't use a proper
open-collector driver for the databus, rather a 74LS245 tri-state buffer (that
drives high signals rather than the line termination) controlled by the SCSI
IO line. The SCSI data bus is terminated by drive 0, while the SCSI RST, SEL,
ACK, and ATN signals are pulled up on the adaptor, therefore terminated there.
SCSI signals RST, SEL, and ACK are driven by proper open-collectors on the
adaptor. The DB25 connector on the adaptor is NOT a standard SCSI DB25.
Connecting anything other than an official Lt. Kernal drive will likely cause
damage to the host adaptor.

 signal  meaning
 ------  -------
 PA 7-0  /Data bus on SCSI (input/output)
 CA2     Pulses SCSI ACK (low to high)

MC6821 Port B connects to mostly the SCSI control bus. PB2.6 controls write
access to 16KiB SRAM. When low (0), no writes are permitted, when high (1)
writes are permitted to $8000-$9FFF AND $E000-$FFFF. Initially, this signal is
low, which keeps the boot ROM mapped to $8000-$9FFF (lower 4KiB for the C64,
upper 4KiB for the C128). Once a high (1) is written, the boot ROM is mapped
out permanently until a reset. CB2 controls which KERNAL is in place. A high
(1) uses the stock KERNAL while a low (0) maps in one of the 8K RAMs as the
KERNAL. The other RAM at $8000-$9FFF memory is ONLY mapped in when PB2.6 is
high (1). The HIRAM line from the PLA goes to the adaptor so it can determine
the proper write action to the main RAM or adaptor RAM. Writing a high (1) to
CB2 while PB2.6 is low (0) queues up a system reset. If CB2 is not set back
to a low (0) or PB2.6 to a high (1), a reset will be asserted in 57 cycles.
On startup, the existing kernal is copied and patched by the LTK DOS. On the
C64, all known kernal versions work, however in C128 mode, only the original
kernal is compatible with the latest LTK DOS (7.2). Make sure to use the
318020-03 kernal (also known as the first version). The LTK DOS overwrites
the new patches to the second and third kernal versions.
The LTK DOS (located at the beginning of the hard drive) is constantly loading
in different modules depending on the actions taken. For example, a call to
the kernal open function will result in the "open" code to be loaded off the
hard disk into the $8000-$9FFF area. This can result in a lot of hard disk
"thrashing" depending on the distance of current LU and the DOS. The LTK
offers the ability to create a "DOS image" (a copy of the DOS, but in a single
file) in each LU to reduce the hard disk seek times when accessing different
parts of LU and the DOS. This feature shouldn't be required in emulation as
the speed of the hard disk access is essentially instantaneous on modern
hardware. Other utilities such as DIR, and VALIDATE, load into $C000-CFFF.
Prior to this, the LTK saves those contents of memory into a reserved space in
the DOS and restores them once the utility has terminated. This may cause some
compatibility problems if you have interrupt or service vectors which point to
that area.

 signal  meaning
 ------  -------
 PB 7    /REQ on SCSI (input)
 PB 6    SRAM control
 PB 5    RST on SCSI (output)
 PB 4    SEL on SCSI (output)
 PB 3    /BUSY on SCSI (input)
 PB 2    /CD on SCSI (input)
 PB 1    /MSG on SCSI (input)
 PB 0    /IO on SCSI (input)
 CB2     KERNAL control

$DF04 - $DF07:

The LTK Port can be between 0 and 15. Only adaptors set to port 0 are allowed
to change the host configuration.

    bit  meaning
    ---  -------
    3-0  LTK port number (input)
    4    Freeze state (0: active, 1: inactive)

*** C128 MMU ***

The LTK cannot work on a real C128 without a MMU daughterboard. This board
is quite a simple in design, but requires a 10 pin ribbon cable back to the
LTK adapter (unlike the CMD SuperCPU daugthercard which has on board registers
to control the MMU behavior). On a C64, there is no ribbon cable, but 2 sets
of pins are shorted, and the HIRAM and CAEC lines are brought to the adapter.
Essentially, this daughterboard intercepts the pin 47 (C128/C64 mode) which
eventually goes to the PLA. The daughtercard processes the HIRAM line from the
CPU as well as the C128/C64, and MS1 lines from the MMU.
When in C64 mode, the HIRAM line is brought to the adapter where a 1 indicates
ROM access, and a 0 RAM access. However in C128 mode, MS1 (0=ROM or EXT
function) from the MMU is inverted and brought back to the adapter.
Once the address and psuedo HIRAM line is processed, the adapter will map
in either the $8000-$9fff or $e000-$ffff using a C64 cartridge mode (eg.
Ultimax) by placing the PLA in 64 mode. Since the output of the MMU to the PLA
is redirected, this can be switched for any memory access. In situations where
the daughtercard is not connected to the adapter, a pullup ensures the MMU
output always reaches the PLA undisturbed.
Other connections back to the adapter will relay that the system is actively
connected to the daughtercard (hence in a C128), so the adapter always boots
up using the second half of the boot ROM, which operates as an external
function ROM in C128 mode. The EXROM and GAME lines are set accordingly so the
system boots into C128 mode.
Given the above behavior, the LTK DOS must boot into C128 mode. To go to C64
mode, the "go64" command can be used, but it does not perform the typical
stock routine. Here, it is redirected to create a C64 stlye boot ROM in RAM,
then the MMU is switched to C64 mode and the PC jumps to the $FFFC vector.
Since the adapter has the ability to reset the system. C128 mode can be
entered via C64 mode (via "go128") as a hard reset is asserted.

*/

#define CART_RAM_SIZE (16 * 1024)

/* largest supported HD in 512 bytes sectors for LTK for DOS up to 7.3 */
#define ltk_imagesize  (32 * 1024 * 1024 * 10 / 512)

static int ltk_inserted = 0;

static uint8_t ltk_rom;
static uint8_t ltk_raml;
static uint8_t ltk_ramh;
static uint8_t ltk_ramwrite;
static uint8_t ltk_freeze;
static uint8_t ltk_on;
static uint8_t ltk_in2;
struct alarm_s *ltk_alarm;
static CLOCK ltk_alarm_time;

/* resources */
static int ltk_io = 1; /* (0=$dexx, 1=$dfxx) */
static int ltk_port = 0;
static char *ltk_serial = NULL;
static char *ltk_disk[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };

static mc6821_state ltk_6821;

/* some prototypes are needed */
static uint8_t ltkernal_io_read(uint16_t addr);
static uint8_t ltkernal_io_peek(uint16_t addr);
static void ltkernal_io_store(uint16_t addr, uint8_t value);
static int ltkernal_io_dump(void);

static io_source_t ltkernal_io_device = {
    CARTRIDGE_NAME_LT_KERNAL, /* name of the device */
    IO_DETACH_CART,           /* use cartridge ID to detach the device when involved in a read-collision */
    IO_DETACH_NO_RESOURCE,    /* does not use a resource for detach */
    0xdf00, 0xdfff, 0x07,     /* range for the device, regs:$df00-$dfff */
    1,                        /* read is always valid */
    ltkernal_io_store,        /* store function */
    NULL,                     /* NO poke function */
    ltkernal_io_read,         /* read function */
    ltkernal_io_peek,         /* peek function */
    ltkernal_io_dump,         /* device state information dump function */
    CARTRIDGE_LT_KERNAL,      /* cartridge ID */
    IO_PRIO_NORMAL,           /* normal priority, device read needs to be checked for collisions */
    0,                        /* insertion order, gets filled in by the registration function */
    IO_MIRROR_NONE            /* NO mirroring */
};

static io_source_list_t *ltkernal_io_list_item = NULL;

static const export_resource_t export_res_plus = {
    CARTRIDGE_NAME_LT_KERNAL, 1, 1, NULL, &ltkernal_io_device, CARTRIDGE_LT_KERNAL
};

static const char ltk_scsi_name[] = {"LTKSCSI"};
static scsi_context_t ltk_scsi;

/* ---------------------------------------------------------------------*/
static int ltkernal_check_scpu64(void)
{
    /* disable LTK completely under scpu64 */
    if ( machine_class == VICE_MACHINE_SCPU64 ) {
        ltk_rom = 0;
        ltk_raml = 0;
        ltk_ramh = 0;
        ltk_freeze = 0;
        ltk_on = 0;
        return 1;
    }

    return 0;
}

static void ltkernal_cancel_alarm(void)
{
    if (ltk_alarm_time != CLOCK_MAX) {
        LOG2((LOG, "LTK RESET CANCELLED"));
        alarm_unset(ltk_alarm);
        ltk_alarm_time = CLOCK_MAX;
    }
}

static void ltkernal_alarm_handler(CLOCK offset, void *data)
{
    ltkernal_cancel_alarm();
    LOG2((LOG, "LTK RESET at 0x%04x", reg_pc));
    machine_trigger_reset(MACHINE_RESET_MODE_POWER_CYCLE);
}

static int ltkernal_registerio(void)
{
    LOG2((LOG, "LTK registerio"));

    if (ltkernal_io_list_item) {
        return 0;
    }

    if (export_add(&export_res_plus) < 0) {
        return -1;
    }

    if (ltk_io < LTKIO_DE00 || ltk_io > LTKIO_DF00) {
        ltk_io = LTKIO_DF00;
    }

    ltkernal_io_device.start_address = 0xde00 + ltk_io * 256;
    ltkernal_io_device.end_address = ltkernal_io_device.start_address + 255;

    LOG1((LOG, "LTK IO is at $%02xxx",
        (unsigned int)(ltk_io == LTKIO_DE00 ? 0xde : 0xdf)));

    ltkernal_io_list_item = io_source_register(&ltkernal_io_device);

    ltk_alarm = alarm_new(maincpu_alarm_context, "LTKResetAlarm", ltkernal_alarm_handler, NULL);
    ltk_alarm_time = CLOCK_MAX;

    return 0;
}

static void ltkernal_unregisterio(void)
{
    LOG2((LOG, "LTK unregisterio"));

    if (!ltkernal_io_list_item) {
        return;
    }

    export_remove(&export_res_plus);
    io_source_unregister(ltkernal_io_list_item);
    ltkernal_io_list_item = NULL;
    alarm_destroy(ltk_alarm);
}

static int set_port(int port, void *param)
{
    if (port < 0 || port > 15) {
        return -1;
    }

    ltk_port = port;
    LOG1((LOG, "LTK port = %d", port));

    return 0;
}

static int set_io(int io, void *param)
{
    if (io < LTKIO_DE00 || io > LTKIO_DF00) {
        return -1;
    }

    ltk_io = io;

    if (ltk_inserted) {
        ltkernal_unregisterio();
        if (ltkernal_registerio()) {
            return -1;
        }
    }

    LOG1((LOG, "LTK IO = %d ($%02xxx)", io,
        (unsigned int)(io == LTKIO_DE00 ? 0xde : 0xdf)));

    return 0;
}

static const resource_int_t resources_int[] = {
    { "LTKport", 0, RES_EVENT_NO, NULL, &ltk_port, set_port, 0 },
    { "LTKio", LTKIO_DF00, RES_EVENT_NO, NULL, &ltk_io, set_io, 0 },
    RESOURCE_INT_LIST_END
};

static int set_image_file(const char *name, void *param)
{
    int i = vice_ptr_to_int(param);

    if (i < 0 || i > 6) {
        return -1;
    }

    util_string_set(&(ltk_disk[i]), name);
    LOG1((LOG, "LTK image[%d] = '%s'", i, name));

    /* apply changes */
    if (ltk_inserted) {
        scsi_image_detach(&ltk_scsi, i << 3);
        scsi_image_attach(&ltk_scsi, i << 3, ltk_disk[i]);
    }

    return 0;
}

static int set_serial(const char *name, void *param)
{
    int l, i, j;

    if (!name) {
        CRIT((LOG, "LTK serial number - nothing provided."));
        return 1;
    }

    l = 0;
    while (name[l]) {
        l++;
    }

    if (l != 8) {
        CRIT((LOG, "LTK serial number '%s' is not 8 digits.", name));
        return 1;
    }

    for (i = 0; i < 8; i++) {
        j = name[i];
        if ( j < '0' || j > '9') {
            CRIT((LOG, "LTK serial number '%s' has invalid character '%c'.",
                name, j));
            return 1;
        }
        ltk_serial[i] = j;
    }

    memcpy(&(roml_banks[10]), ltk_serial, 8);
    memcpy(&(roml_banks[4096 + 10]), ltk_serial, 8);
    LOG1((LOG, "LTK serial = '%s'", name));

    return 0;
}

static const resource_string_t resources_string[] = {
    { "LTKimage0", "", RES_EVENT_NO, NULL,
      &(ltk_disk[0]), set_image_file, (void *)0 },
    { "LTKimage1", "", RES_EVENT_NO, NULL,
      &(ltk_disk[1]), set_image_file, (void *)1 },
    { "LTKimage2", "", RES_EVENT_NO, NULL,
      &(ltk_disk[2]), set_image_file, (void *)2 },
    { "LTKimage3", "", RES_EVENT_NO, NULL,
      &(ltk_disk[3]), set_image_file, (void *)3 },
    { "LTKimage4", "", RES_EVENT_NO, NULL,
      &(ltk_disk[4]), set_image_file, (void *)4 },
    { "LTKimage5", "", RES_EVENT_NO, NULL,
      &(ltk_disk[5]), set_image_file, (void *)5 },
    { "LTKimage6", "", RES_EVENT_NO, NULL,
      &(ltk_disk[6]), set_image_file, (void *)6 },
    { "LTKserial", "87000000", RES_EVENT_NO, NULL,
      &ltk_serial, set_serial, (void *)0 },
    RESOURCE_STRING_LIST_END
};

static const cmdline_option_t cmdline_options[] =
{
    { "-ltkimage0", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKimage0", NULL,
      "<Name>", "Specify name of LTK image file 0" },
    { "-ltkimage1", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKimage1", NULL,
      "<Name>", "Specify name of LTK image file 1" },
    { "-ltkimage2", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKimage2", NULL,
      "<Name>", "Specify name of LTK image file 2" },
    { "-ltkimage3", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKimage3", NULL,
      "<Name>", "Specify name of LTK image file 3" },
    { "-ltkimage4", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKimage4", NULL,
      "<Name>", "Specify name of LTK image file 4" },
    { "-ltkimage5", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKimage5", NULL,
      "<Name>", "Specify name of LTK image file 5" },
    { "-ltkimage6", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKimage6", NULL,
      "<Name>", "Specify name of LTK image file 6" },
    { "-ltkserial", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKserial", NULL,
      "<Name>", "Specify LTK serial number (XXXXXXXX) to override the ROM" },
    { "-ltkport", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKport", NULL,
      "<value>", "Set LTK port number (0..15)" },
    { "-ltkio", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "LTKio", NULL,
      "<value>", "Set LTK IO page (0=$DExx, 1=$DFxx=default)" },
    CMDLINE_LIST_END
};

int ltkernal_cmdline_options_init(void)
{
    if (cmdline_register_options(cmdline_options) < 0) {
        return -1;
    }

    return 0;
}

int ltkernal_resources_init(void)
{
    int i;

    LOG2((LOG, "LTK resource init"));

    if (ltkernal_check_scpu64()) {
        return 0;
    }

    ltk_serial = lib_strdup("00000000");

    for (i = 0; i < 7; i++) {
        ltk_disk[i] = NULL;
    }
    if (resources_register_string(resources_string) < 0) {
        return -1;
    }
    if (resources_register_int(resources_int) < 0) {
        return -1;
    }

    return 0;
}

int ltkernal_resources_shutdown(void)
{
    int i;

    LOG2((LOG, "LTK resource shutdown"));

    for (i = 0; i < 7; i++) {
        if (ltk_disk[i]) {
            lib_free(ltk_disk[i]);
        }
        ltk_disk[i] = NULL;
    }

    if (ltk_serial) {
        lib_free(ltk_serial);
    }
    ltk_serial = NULL;

    return 0;
}

static void ltk_imageopenall(void)
{
    int32_t i;

    /* purge out any old or stall file handles */
    for (i = 0; i < 56; i++) {
        ltk_scsi.file[i] = NULL;
    }

    /* setup new ones */
    for (i = 0; i < 7; i++) {
        scsi_image_attach(&ltk_scsi, i << 3, ltk_disk[i]);
    }

    return;
}

/* ---------------------------------------------------------------------*/

static uint8_t ltk_get_pa(mc6821_state *ctx)
{
    uint8_t val;
    scsi_context_t *scsi = (scsi_context_t*)ctx->p;

    val = scsi_get_bus(scsi);
    IDBG((LOG, "GET PA 0x%02x", val));

    return val;
}

static uint8_t ltk_get_pb(mc6821_state *ctx)
{
    uint8_t val;
    scsi_context_t *scsi = (scsi_context_t*)ctx->p;

    scsi_process_noack(scsi);
    ltk_in2 = (ltk_in2 & 0x70) | ((!scsi->req) << 7) | ((!scsi->bsyo) << 3) |
        ((!scsi->cd) << 2) | ((!scsi->msg)<<1) | (!scsi->io);
    val = ltk_in2;

    return val;
}

static void ltk_set_ca2(mc6821_state *ctx)
{
    scsi_context_t *scsi = (scsi_context_t*)ctx->p;

    IDBG((LOG, "SET CA2 called at %04x", reg_pc));

    if (ctx->CA2) {
        scsi_process_ack(scsi);
    }
}

static uint8_t ltkernal_io_read(uint16_t addr)
{
    int port, reg;
    uint8_t val = 0;

    if (ltk_on) {
        IDBG((LOG, "--------------------"));

        if (addr & 0x4) {
            val = (ltk_port & 15) | ( (!ltk_freeze) << 4 );
        } else {
            port = (addr >> 1) & 1; /* rs1 */
            reg = (addr >> 0) & 1;  /* rs0 */
            val = mc6821core_read(&ltk_6821, port /* rs1 */, reg /* rs0 */);
        }

        IDBG((LOG, "LTK io r %04x = %02x at 0x%04x", addr, val, reg_pc));
    }

    return val;
}

static uint8_t ltkernal_io_peek(uint16_t addr)
{
    int port, reg;
    uint8_t val = 0;

    if (ltk_on) {
        if (addr & 0x4) {
            val = (ltk_port & 15) | ( (!ltk_freeze) << 4 );
        } else {
            port = (addr >> 1) & 1; /* rs1 */
            reg = (addr >> 0) & 1;  /* rs0 */
            val = mc6821core_peek(&ltk_6821, port /* rs1 */, reg /* rs0 */);
        }

        IDBG((LOG, "LTK io p %04x = %02x at 0x%04x", addr, val, reg_pc));
    }

    return val;
}

static void ltk_update_memflags(mc6821_state *ctx)
{
    uint8_t temp;
    temp = (ctx->dataB & ctx->ddrB) | (ltk_in2 & (ctx->ddrB ^ 0xff));

    ltk_ramwrite = (temp & 0x40) != 0;
    /* once the lower ram is turned on ROM is permanently turned off */
    if (ltk_ramwrite) {
        ltk_rom = 0;
    }

    ltk_ramh = (!ctx->CB2) && (!ltk_rom);
    /* low ram is only on when replacement kernal is on */
    ltk_raml = ltk_ramh && ltk_ramwrite;

    cart_port_config_changed_slotmain();
}

static void ltk_set_pa(mc6821_state *ctx)
{
    scsi_context_t *scsi = (scsi_context_t*)ctx->p;

    if (scsi_set_bus(scsi, ctx->dataA)) {
        if (ctx->ctrlA & MC6821_CTRL_REG) {
            CRIT((LOG, "Cannot set bus to %02x at %04x", ctx->dataA, reg_pc));
        }
    }
}

static void ltk_set_pb(mc6821_state *ctx)
{
    static uint16_t last = -1;
    scsi_context_t *scsi = (scsi_context_t*)ctx->p;

    if (last == ctx->dataB) return;
    last = ctx->dataB;

    scsi->sel = (ctx->dataB & 0x10) != 0;
    scsi->rst = (ctx->dataB & 0x20) != 0;
    scsi_process_noack(scsi);

    if (ctx->dataB & 0x40) {
        ltkernal_cancel_alarm();
        IDBG((LOG, "LTK RAM ON at 0x%04x", reg_pc));
    } else {
        IDBG((LOG, "LTK RAM OFF at 0x%04x", reg_pc));
    }

    ltk_update_memflags(ctx);
}

static void ltk_set_cb2(mc6821_state *ctx)
{
    if (ctx->CB2) {
        IDBG((LOG, "LTK STOCK KERNAL ON at 0x%04x", reg_pc));
    } else {
        ltkernal_cancel_alarm();
        IDBG((LOG, "LTK STOCK KERNAL OFF at 0x%04x", reg_pc));
    }

    ltk_update_memflags(ctx);

    if ( !ltk_ramwrite && ctx->CB2) {
        if (ltk_alarm_time == CLOCK_MAX) {
            /* reset is scheduled in the next 57 cycles */
            LOG2((LOG, "LTK RESET SCHEDULED at 0x%04x", reg_pc));
            ltk_alarm_time = maincpu_clk + 57;
            alarm_set(ltk_alarm, ltk_alarm_time);
        }
    }
}

static void ltkernal_io_store(uint16_t addr, uint8_t value)
{
    int port, reg;

    if (ltk_on) {
        IDBG((LOG, "--------------------"));
        if (addr & 0x4) {
            /* any writes to 4, 5, 6 or 7 turn off LTK I/O until reset */
/* it turns out nothing happens when you write here. */
/*
            if (ltk_ramwrite) {
                ltk_on = 0;
                ltk_ramwrite = 0;
                ltk_rom = 0;
                ltk_raml = 0;
                ltk_ramh = 0;
                LOG1((LOG, "LTK OFF at %04x", reg_pc));
            }
*/
        } else {
            port = (addr >> 1) & 1; /* rs1 */
            reg = (addr >> 0) & 1;  /* rs0 */
            mc6821core_store(&ltk_6821, port /* rs1 */, reg /* rs0 */, value);
        }

        IDBG((LOG, "LTK io w %04x < %02x at 0x%04x", addr, value, reg_pc));
    }
}

static int ltkernal_io_dump(void)
{
    mon_out("IO mapped?: %s\n", ltk_on ? "Yes" : "No");
    mon_out("IO location: $%xxx\n", (unsigned int)(ltk_io == 0 ? 0xde : 0xdf ));
    mon_out("Port number: %d\n", ltk_port);
    mon_out("Freeze state: %s\n", ltk_freeze ? "Yes" : "No");
    mon_out("ROM mapped to $8000-$9FFF?: %s\n", ltk_rom ? "Yes" : "No");
    mon_out("RAM mapped to $8000-$9FFF?: %s\n", ltk_raml ? "Yes" : "No");
    mon_out("RAM mapped to $E000-$FFFF?: %s\n", ltk_ramh ? "Yes" : "No");
    mon_out("RAM write enabled?: %s\n", ltk_ramwrite ? "Yes" : "No");

    mon_out("MC6821\n");
    mc6821core_dump(&ltk_6821);

    return 0;
}

/* ---------------------------------------------------------------------*/
int c128ltkernal_mmu_translate(unsigned int addr, uint8_t **base, int *start, int *limit, int mem_config)
{
    /* unlike the c64 mmu_translate, here we only apply what we can and move
       on. return a 1 if we did, or 0 if we didn't apply anything. */
    if (addr >= 0x8000 && addr <= 0x9fff) {
        /* only map in ROM if external ROM access */
        if (ltk_rom && ((mem_config & 0x30) == 0x20)) {
            *base = roml_banks + 0x1000 - 0x8000;
            *start = 0x8000;
            *limit = 0x9ffd;
            return 1;
        } else if (ltk_raml) {
            /* RAM maps in whenever ltk_raml is active */
            *base = export_ram0 - 0x8000;
            *start = 0x8000;
            *limit = 0x9ffd;
            return 1;
        }
    } else if (addr >= 0xe000) {
        /* map in if either ROM or EXT FUNCTION; this is what the MMU
           daughtercard checks for. */
        if ((ltk_ramh && ((mem_config & 0x20) == 0x00)) || (ltk_ramh && ltk_freeze )) {
            *base = export_ram0 + 0x2000 - 0xe000;
            *start = 0xe000;
            *limit = 0xfffd;
            return 1;
        }
    }

    /* nothing to apply here */
    return 0;
}

uint8_t c128ltkernal_ram_read(uint16_t addr, uint8_t *value)
{
/* called by all memory reads so limit it */
    if (addr >= 0x8000 && addr <= 0x9fff) {
        if (ltk_raml) {
            *value = export_ram0[(addr & 0x1fff)];
            MDBG((LOG, "LTK c128ltkernal_ram_read(RAM)  %04x = %02x",
                (int)addr, (int)*value));
            return 1;
        }
    }

    return 0;
}

uint8_t c128ltkernal_ram_store(uint16_t addr, uint8_t value)
{
/* called by all memory writes so limit it */
    if (addr >= 0x8000 && addr <= 0x9fff) {
        if (ltk_raml && ltk_ramwrite) {
            export_ram0[(addr & 0x1fff)] = value;
            MDBG((LOG, "LTK c128ltkernal_ram_store(RAM) %04x = %02x",
                (int)addr, (int)value));
            return 1;
        }
    }

    return 0;
}

uint8_t c128ltkernal_roml_read(uint16_t addr, uint8_t *value)
{
/* also called by $c000-$dfff memory so limit it */
    if (addr > 0x9fff || addr < 0x8000) {
        return 0;
    }
    if (ltk_rom) {
/* for 128, the LTK upper 8K of ROM is used; only map that */
        *value = roml_banks[(addr & 0x0fff)|0x1000];
        MDBG((LOG, "LTK c128ltkernal_roml_read(ROM) %04x = %02x",
            (int)addr, (int)*value));
        return 1;
    } else if (ltk_raml) {
        *value = export_ram0[(addr & 0x1fff)];
        MDBG((LOG, "LTK c128ltkernal_roml_read(RAM) %04x = %02x",
            (int)addr, (int)*value));
        return 1;
    }
    return 0;
}

uint8_t c128ltkernal_roml_store(uint16_t addr, uint8_t value)
{
    if (addr > 0x9fff || addr < 0x8000) {
        return 0;
    }
    if (ltk_raml && ltk_ramwrite) {
        export_ram0[(addr & 0x1fff)] = value;
        MDBG((LOG, "LTK c128ltkernal_roml_store %04x = %02x",
            (int)addr, (int)value));
        return 1;
    }
    return 0;
}

uint8_t c128ltkernal_basic_hi_read(uint16_t addr, uint8_t *value)
{
    if (addr > 0x9fff || addr < 0x8000) {
        return 0;
    }
    if (ltk_raml) {
        *value = export_ram0[(addr & 0x1fff)];
        MDBG((LOG, "LTK c128ltkernal_basic_hi_read %04x = %02x", (int)addr,
            (int)*value));
        return 1;
    }
    return 0;
}

uint8_t c128ltkernal_basic_hi_store(uint16_t addr, uint8_t value)
{
    if (addr > 0x9fff || addr < 0x8000) {
        return 0;
    }
    if (ltk_raml && ltk_ramwrite) {
        export_ram0[(addr & 0x1fff)] = value;
        MDBG((LOG, "LTK c128ltkernal_basic_hi_store %04x = %02x", (int)addr,
            (int)value));
        return 1;
    }
    return 0;
}

uint8_t c128ltkernal_hi_read(uint16_t addr, uint8_t *value)
{
    if (ltk_ramh) {
        *value = export_ram0[0x2000|(addr & 0x1fff)];
        MDBG((LOG, "LTK c128ltkernal_hi_read %04x = %02x", (int)addr, (int)*value));
        return 1;
    }
    return 0;
}

uint8_t c128ltkernal_hi_store(uint16_t addr, uint8_t value)
{
    if (ltk_ramh && ltk_ramwrite) {
        export_ram0[0x2000|(addr & 0x1fff)] = value;
        MDBG((LOG, "LTK c128ltkernal_hi_store %04x = %02x", (int)addr, (int)value));
        return 1;
    }
    return 0;
}

void c128ltkernal_switch_mode(int mode)
{
    LOG2((LOG, "LTK switch mode %d", mode));

    if ( mode ) {
        /* reconfigure for c64 mode */
        cart_config_changed_slotmain(CMODE_8KGAME, CMODE_ULTIMAX, CMODE_READ |
            CMODE_PHI2_RAM);
    } else {
        /* reconfigure for c128 mode; boot via ext function rom */
        cart_config_changed_slotmain(CMODE_RAM, CMODE_RAM, CMODE_READ);
    }
}

int ltkernal_mmu_translate(unsigned int addr, uint8_t **base, int *start, int *limit)
{
    /* the callers take care of most stuff here, but since this is both a
       standard cart and ultimax, we have to handle those situations.
       NOTE that the caller doesn't process the return value, so we have to
       have some values set for base, start, and limit, which we do if all
       else fails. */
    if (addr >= 0x8000 && addr <= 0x9fff) {
        if (ltk_rom) {
            *base = roml_banks - 0x8000;
        } else if (ltk_raml) {
            *base = export_ram0 - 0x8000;
        } else {
            /* include RAM to speed things up */
            *base = mem_ram;
        }
        *start = 0x8000;
        *limit = 0x9ffd;
        return CART_READ_VALID;
    } else if (addr >= 0xe000) {
        int p = (pport.dir & pport.data) | (~pport.dir & 7);
        if ((ltk_ramh && (p & 2)) || (ltk_ramh && ltk_freeze )) {
            *base = export_ram0 + 0x2000 - 0xe000;
        } else if (p & 2) {
#if 0
            *base = c64memrom_kernal64_trap_rom - 0xe000;
#else
            *base = (uint8_t*)(0 - 0xe000);
            *base += (uintptr_t)c64memrom_kernal64_trap_rom;
#endif
        } else {
            /* include RAM to speed things up */
            *base = mem_ram;
        }
        *start = 0xe000;
        *limit = 0xfffd;
        return CART_READ_VALID;
    }

    /* nothing to apply here, so use the slow mode */
    *base = NULL;
    *start = 0;
    *limit = 0;
    return CART_READ_THROUGH;
}

uint8_t ltkernal_roml_read(uint16_t addr)
{
    uint8_t val;
    if (ltk_rom) {
        val = roml_banks[(addr & 0x1fff)];
    } else if (ltk_raml) {
        val = export_ram0[(addr & 0x1fff)];
    } else {
        val = mem_read_without_ultimax(addr);
    }
    MDBG((LOG, "LTK roml_read %04x = %02x", addr, val));
    return val;
}

uint8_t ltkernal_romh_read(uint16_t addr)
{
    uint8_t val;
    int p = (pport.dir & pport.data) | (~pport.dir & 7);
    if ((ltk_ramh && (p & 2)) || (ltk_ramh && ltk_freeze )) {
        val = export_ram0[0x2000|(addr & 0x1fff)];
    } else {
        val = mem_read_without_ultimax(addr);
    }
    MDBG((LOG, "LTK romh_read %04x = %02x roml=%d romh=%d romw=%d pport=%02x",
        (int)addr, (int)val, (int)ltk_raml, (int)ltk_ramh, (int)ltk_ramwrite,
        (int)pport.data));
    return val;
}

void ltkernal_roml_store(uint16_t addr, uint8_t value)
{
    if (ltk_rom) {
        ram_store(addr, value);
    } else if (ltk_raml && ltk_ramwrite) {
        export_ram0[(addr & 0x1fff)] = value;
    } else {
        ram_store(addr, value);
    }
}

void ltkernal_romh_store(uint16_t addr, uint8_t value)
{
    if (ltk_ramh && ltk_ramwrite) {
        export_ram0[0x2000|(addr & 0x1fff)] = value;
    } else {
        ram_store(addr, value);
    }
    MDBG((LOG, "LTK romh_store %04x = %02x roml=%d romh=%d romw=%d pport=%02x",
        (int)addr, (int)value, (int)ltk_raml, (int)ltk_ramh, (int)ltk_ramwrite,
        (int)pport.data));
}

int ltkernal_peek_mem(export_t *ex, uint16_t addr, uint8_t *value)
{
    int p;
    if (ltk_rom) {
        if (addr >= 0x8000 && addr <= 0x9fff) {
            *value = roml_banks[addr & 0x1fff];
            return CART_READ_VALID;
        }
    }
    if (ltk_raml) {
        if (addr >= 0x8000 && addr <= 0x9fff) {
            *value = export_ram0[addr & 0x1fff];
            return CART_READ_VALID;
        }
    }
    if (ltk_ramh) {
        if (addr >= 0xe000) {
            *value = export_ram0[0x2000|(addr & 0x1fff)];
            return CART_READ_VALID;
        }
    }
/* Workaround needed when using monitor */
    p = (pport.dir & pport.data) | (~pport.dir & 7);
    if (((p & 3) == 3) && (addr >= 0xa000 && addr <= 0xbfff)) {
        *value = c64memrom_basic64_rom[addr & 0x1fff];
        return CART_READ_VALID;
    }
    if (((p & 2) == 2) && (addr >= 0xe000)) {
        *value = c64memrom_kernal64_rom[addr & 0x1fff];
        return CART_READ_VALID;
    }

    return CART_READ_THROUGH;
}

/* ---------------------------------------------------------------------*/

/* FIXME: this still needs to be tweaked to match the hardware */
static RAMINITPARAM ramparam = {
    .start_value = 255,
    .value_invert = 2,
    .value_offset = 1,

    .pattern_invert = 0x100,
    .pattern_invert_value = 255,

    .random_start = 0,
    .random_repeat = 0,
    .random_chance = 0,
};

void ltkernal_powerup(void)
{
    ram_init_with_pattern(export_ram0, CART_RAM_SIZE, &ramparam);
}

void ltkernal_freeze(void)
{
    MDBG((LOG, "LTK roml=%d romh=%d romw=%d pport=%02x", (int)ltk_raml,
        (int)ltk_ramh, (int)ltk_ramwrite, (int)pport.data));
    if (ltk_ramh) {
        LOG2((LOG, "LTK freeze"));

        ltk_freeze = 1;
        ltk_rom = 0;
        ltk_raml = 1;
        ltk_ramh = 1;
        ltk_ramwrite = 1;
    } else {
        LOG1((LOG, "LTK freeze but no LTK kernal in place; ignoring"));
    }
    cartridge_release_freeze();
}

void ltkernal_config_init(void)
{
    int32_t i;
    LOG2((LOG, "LTK config init"));

    if (ltkernal_check_scpu64()) {
        return;
    }

    /* set default cart mode depending on machine type */
    if ( machine_class == VICE_MACHINE_C64SC ||
        machine_class == VICE_MACHINE_C64 ) {
        c128ltkernal_switch_mode(1);
    } else {
        /* must be a c128 at this point */
        c128ltkernal_switch_mode(0);
    }

    for (i = 0; i < 0x2000; i++) {
        export_ram0[i] = (i >> 8) + 0x80;
        export_ram0[0x2000|i] = (i >> 8) + 0xe0;
    }

    /* defaults */
    /* ROM is on */
    ltk_rom = 1;
    ltk_raml = 0;
    ltk_ramh = 0;

    /* stop 6821 from calling CA2 during reset */
    ltk_6821.set_ca2 = NULL;
    ltk_6821.set_pa = NULL;
    ltk_6821.set_pb = NULL;
    ltk_6821.set_cb2 = NULL;
    ltk_6821.get_pa = NULL;
    ltk_6821.get_pb = NULL;
    mc6821core_reset(&ltk_6821);
    ltk_6821.set_ca2 = ltk_set_ca2;
    ltk_6821.set_pa = ltk_set_pa;
    ltk_6821.set_pb = ltk_set_pb;
    ltk_6821.set_cb2 = ltk_set_cb2;
    ltk_6821.get_pa = ltk_get_pa;
    ltk_6821.get_pb = ltk_get_pb;
    ltk_6821.p = &ltk_scsi;

    /* SCSI register defaults */
    ltk_scsi.atn = 0;
    ltk_scsi.rst = 0;
    ltk_scsi.bsyi = 0;
    ltk_scsi.ack = 0;
    ltk_scsi.myname = (char *)ltk_scsi_name;

    ltk_in2 = 0xff - 0x40; /* make sure RAM is not enabled */

    /* no freeze state */
    ltk_freeze = 0;

    /* ltk is mapped */
    ltk_on = 1;

    ltk_imageopenall();

    scsi_process_noack(&ltk_scsi);
}

void ltkernal_config_setup(uint8_t *rawcart)
{
    LOG2((LOG, "LTK config setup"));

    /* copy supplied ROM image to memory */
    memcpy(roml_banks, rawcart, 0x2000);

    /* copy in the LTK serial number */
    memcpy(&(roml_banks[10]), ltk_serial, 8);
    memcpy(&(roml_banks[4096 + 10]), ltk_serial, 8);

    /* set default cart mode depending on machine type */
    if ( machine_class == VICE_MACHINE_C64SC ||
        machine_class == VICE_MACHINE_C64 ) {
        c128ltkernal_switch_mode(1);
    } else {
        /* must be a c128 at this point */
        c128ltkernal_switch_mode(0);
    }
}

/* ---------------------------------------------------------------------*/

static int ltkernal_common_attach(void)
{
    LOG2((LOG, "LTK common attach"));

    if (ltkernal_check_scpu64()) {
        return -1;
    }

    scsi_reset(&ltk_scsi);
    ltk_scsi.max_imagesize = ltk_imagesize;
    ltk_scsi.limit_imagesize = ltk_imagesize;
    ltk_scsi.msg_after_status = 1;

    if (ltkernal_registerio() < 0) {
        return -1;
    }
    ltk_inserted = 1;
    return 0;
}

int ltkernal_crt_attach(FILE *fd, uint8_t *rawcart)
{
    crt_chip_header_t chip;
    int i;

    if (ltkernal_check_scpu64()) {
        return -1;
    }

    for (i = 0; i <= 0; i++) {
        if (crt_read_chip_header(&chip, fd)) {
            return -1;
        }

        if (chip.bank != 0 || chip.size != 0x2000) {
            return -1;
        }

        if (crt_read_chip(rawcart, chip.bank << 13, &chip, fd)) {
            return -1;
        }
    }

    return ltkernal_common_attach();
}

int ltkernal_bin_attach(const char *filename, uint8_t *rawcart)
{
    LOG2((LOG, "LTK bin attach"));

    if (ltkernal_check_scpu64()) {
        return -1;
    }

    if (util_file_load(filename, rawcart, 0x2000,
        UTIL_FILE_LOAD_SKIP_ADDRESS) < 0) {
        return -1;
    }

    return ltkernal_common_attach();
}

void ltkernal_detach(void)
{
    LOG2((LOG, "LTK detach"));

    scsi_image_detach_all(&ltk_scsi);

    ltkernal_unregisterio();
    ltk_inserted = 0;
}

/* ---------------------------------------------------------------------*/

/* CARTLTK snapshot module format:

   type   | name          | description
   ------------------------------------
   BYTE   | rom           | ltk_rom
   BYTE   | raml          | ltk_raml
   BYTE   | ramh          | ltk_ramh
   BYTE   | ramwrite      | ltk_ramwrite
   BYTE   | freeze        | ltk_freeze
   BYTE   | on            | ltk_on
   BYTE   | in2           | ltk_in2
   BYTE   | io            | ltk_io
   BYTE   | port          | ltk_port
   DWORD  | alarm_time    | ltk_alarm_time
   ARRAY  | ROML          | 8192 BYTES of ROML data (boot rom, $8000-$9FFF)
   ARRAY  | export_ram0   | 16384 BYTES of export RAM data (RAML & RAMH)
   MC6821 | SNAPSHOT6821  | ltk_6821
   SCSI   | SNAPSHOTSCSI  | ltk_scsi

*/

static const char snap_module_name[] = "CARTLTK";
#define SNAP_MAJOR   0
#define SNAP_MINOR   0

int ltkernal_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, snap_module_name, SNAP_MAJOR, SNAP_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (0
        || (SMW_B(m, ltk_rom) < 0)
        || (SMW_B(m, ltk_raml) < 0)
        || (SMW_B(m, ltk_ramh) < 0)
        || (SMW_B(m, ltk_ramwrite) < 0)
        || (SMW_B(m, ltk_freeze) < 0)
        || (SMW_B(m, ltk_on) < 0)
        || (SMW_B(m, ltk_in2) < 0)
        || (SMW_DW(m, ltk_io) < 0)
        || (SMW_DW(m, ltk_port) < 0)
        || (SMW_CLOCK(m, ltk_alarm_time) < 0)
        || (SMW_BA(m, roml_banks, 0x2000) < 0)
        || (SMW_BA(m, export_ram0, 0x4000) < 0)) {
        goto fail;
    }

    if (mc6821core_snapshot_write_data(&ltk_6821, m) < 0) {
        goto fail;
    }

    snapshot_module_close(m);

    return scsi_snapshot_write_module(&ltk_scsi, s);

fail:
    snapshot_module_close(m);
    return -1;
}

int ltkernal_snapshot_read_module(snapshot_t *s)
{
    uint8_t vmajor, vminor;
    snapshot_module_t *m;

    m = snapshot_module_open(s, snap_module_name, &vmajor, &vminor);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than current */
    if (snapshot_version_is_bigger(vmajor, vminor, SNAP_MAJOR, SNAP_MINOR)) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        goto fail;
    }

    ltkernal_detach();

    if (0
        || (SMR_B(m, &ltk_rom) < 0)
        || (SMR_B(m, &ltk_raml) < 0)
        || (SMR_B(m, &ltk_ramh) < 0)
        || (SMR_B(m, &ltk_ramwrite) < 0)
        || (SMR_B(m, &ltk_freeze) < 0)
        || (SMR_B(m, &ltk_on) < 0)
        || (SMR_B(m, &ltk_in2) < 0)
        || (SMR_DW_INT(m, &ltk_io) < 0)
        || (SMR_DW_INT(m, &ltk_port) < 0)
        || (SMR_CLOCK(m, &ltk_alarm_time) < 0)
        || (SMR_BA(m, roml_banks, 0x2000) < 0)
        || (SMR_BA(m, export_ram0, 0x4000) < 0)) {
        goto fail;
    }

    if (mc6821core_snapshot_read_data(&ltk_6821, m) < 0) {
        goto fail;
    }

    snapshot_module_close(m);

    if (scsi_snapshot_read_module(&ltk_scsi, s) < 0) {
        return -1;
    }

    ltk_imageopenall();

    if (ltk_alarm_time < CLOCK_MAX) {
        alarm_set(ltk_alarm, ltk_alarm_time);
    } else {
        alarm_unset(ltk_alarm);
    }

    return ltkernal_common_attach();

fail:
    snapshot_module_close(m);
    return -1;
}
