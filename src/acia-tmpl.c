/*
 *
 * This file is part of Commodore 64 emulator.
 * See README for copyright notice
 *
 * ACIA 6551 rs232 emulation
 *
 * Written by
 *    Andre Fachat (a.fachat@physik.tu-chemnitz.de)
 *
 */

#include <stdio.h>

#include "resources.h"
#include "cmdline.h"
#include "vice.h"
#include "types.h"
#include "vmachine.h"
#include "interrupt.h"
#include "rs232.h"
#include "acia.h"

INCLUDES

#undef	DEBUG

/*
 * FIXME: ACIA_TICKS should vary according to
 *  - rs232 baud rate and
 *  - CPU clock frequency.
 * Where do we get the latter?
 */

#define	ACIA_TICKS	21111

static int fd = -1;
static int intx = 0;	/* indicates that a transmit is currently ongoing */
static int irq = 0;
static BYTE cmd;
static BYTE ctrl;
static BYTE rxdata;	/* data that has been received last */
static BYTE txdata;	/* data prepared to send */
static BYTE status;

/******************************************************************/

/* rs232.h replacement functions if no rs232 device available */

#ifndef HAVE_RS232

static int rs232_open(int device)
{
    return -1;
}

static void rs232_close(int fd)
{
}

static int rs232_putc(int fd, BYTE b)
{
    return -1;
}

static int rs232_getc(int fd, BYTE *b)
{
    return -1;
}

#endif

/******************************************************************/

static int myacia_device;
static int myacia_irq;

static int myacia_set_device(resource_value_t v) {

    if(fd>=0) {
	fprintf(stderr, "MYACIA: device open, change effective only after "
		"close!\n");
    }
    myacia_device = (int) v;
    return 0;
}

static int myacia_set_irq(resource_value_t v) {
    int new_irq = (int)v;

    if(myacia_irq != new_irq) {
	mycpu_set_int(I_MYACIA, 0);
	if(irq) {
	    mycpu_set_int(I_MYACIA, new_irq);
	}
    }
    myacia_irq = new_irq;

    return 0;
}

static resource_t resources[] = {
    { "MyAciaDev", RES_INTEGER, (resource_value_t) MyDevice,
      (resource_value_t *) & myacia_device, myacia_set_device },
    { "MyAciaIrq", RES_INTEGER, (resource_value_t) MyIrq,
      (resource_value_t *) & myacia_irq, myacia_set_irq },
    { NULL }
};

int myacia_init_resources(void) {
    return resources_register(resources);
}

static cmdline_option_t cmdline_options[] = {
    { "-myaciadev", SET_RESOURCE, 1, NULL, NULL, "MyAciaDev", NULL,
	"<0-3>", "Specify RS232 device this ACIA should work on" },
    { NULL }
};

int myacia_init_cmdline_options(void) {
    return cmdline_register_options(cmdline_options);
}

/******************************************************************/

void reset_myacia(void) {

#ifdef DEBUG
	printf("reset_myacia\n");
#endif

	cmd = 0;
	ctrl = 0;
	status = 0x10;
	intx = 0;

	if(fd>=0) rs232_close(fd);
	fd = -1;

	mycpu_unset_alarm(A_MYACIA);
	mycpu_set_int(I_MYACIA, 0);
	irq = 0;
}

/* -------------------------------------------------------------------------- */

/* The dump format has a module header and the data generated by the
 * chip...
 *
 * The version of this dump description is 0/0
 */

#define ACIA_DUMP_VER_MAJOR      0
#define ACIA_DUMP_VER_MINOR      0

/*
 * The dump data:
 *
 * UBYTE	TDR	Transmit Data Register
 * UBYTE	RDR	Receiver Data Register
 * UBYTE	SR	Status Register (includes state of IRQ line)
 * UBYTE	CMD	Command Register
 * UBYTE	CTRL	Control Register
 *
 * UBYTE	INTX	0 = no data to tx; 2 = TDR valid; 1 = in transmit
 *
 * DWORD	TICKS	ticks till the next TDR empty interrupt
 */

/* FIXME!!!  Error check.  */
/* FIXME!!!  If no connection, emulate carrier lost or so */
int myacia_write_snapshot_module(snapshot_t * p)
{
    snapshot_module_t *m;
    int byte;

    m = snapshot_module_create(p, "MYACIA",
                               ACIA_DUMP_VER_MAJOR, ACIA_DUMP_VER_MINOR);
    if (m == NULL)
        return -1;

    snapshot_module_write_byte(m, txdata);
    snapshot_module_write_byte(m, rxdata);
    snapshot_module_write_byte(m, status | (irq?0x80:0));
    snapshot_module_write_byte(m, cmd);
    snapshot_module_write_byte(m, ctrl);
    snapshot_module_write_byte(m, intx);
    snapshot_module_write_dword(m, (mycpu_int_status.alarm_clk[A_MYACIA]
                                    - myclk));

    snapshot_module_close(m);

    return 0;
}

int myacia_read_snapshot_module(snapshot_t * p)
{
    char name[SNAPSHOT_MODULE_NAME_LEN];
    BYTE vmajor, vminor;
    BYTE byte;
    DWORD dword;
    snapshot_module_t *m;

    m = snapshot_module_open(p, name, &vmajor, &vminor);
    if (m == NULL)
        return -1;

    if (strcmp(name, "MYACIA") || vmajor != ACIA_DUMP_VER_MAJOR) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_read_byte(m, &txdata);
    snapshot_module_read_byte(m, &rxdata);

    irq = 0;
    snapshot_module_read_byte(m, &status);
    if(status & 0x80) {
	status &= 0x7f;
	irq = 1;
	set_int_noclk(&mycpu_int_status, I_MYACIA, myacia_irq);
    } else {
	set_int_noclk(&mycpu_int_status, I_MYACIA, 0);
    }

    snapshot_module_read_byte(m, &cmd);
    snapshot_module_read_byte(m, &ctrl);

    snapshot_module_read_byte(m, &byte);
    intx = byte;

    snapshot_module_read_dword(m, &dword);
    mycpu_set_alarm(A_MYACIA, dword);

    if (snapshot_module_close(m) < 0)
        return -1;

    return 0;
}


void REGPARM2 store_myacia(ADDRESS a, BYTE b) {

#ifdef DEBUG
	printf("store_myacia(%04x,%02x\n",a,b);
#endif

	switch(a & 3) {
	case ACIA_DR:
		txdata = b;
		if(cmd&1) {
		  if(!intx) {
		    mycpu_set_alarm(A_MYACIA, 1);
		    intx = 2;
		  } else
		  if(intx==1) {
		    intx++;
		  }
		  status &= 0xef;		/* clr TDRE */
		}
		break;
	case ACIA_SR:
		if(fd>=0) rs232_close(fd);
		fd = -1;
		status &= ~4;
		cmd &= 0xe0;
		intx = 0;
		mycpu_set_int(I_MYACIA, 0);
		irq = 0;
		mycpu_unset_alarm(A_MYACIA);
		break;
	case ACIA_CTRL:
		ctrl = b;
		/* TODO: use baudrate for int_acia rate */
		break;
	case ACIA_CMD:
		cmd = b;
		if((cmd & 1) && (fd<0)) {
		  fd = rs232_open(myacia_device);
		  mycpu_set_alarm(A_MYACIA, ACIA_TICKS);
		} else
		if(fd>=0 && !(cmd&1)) {
		  rs232_close(fd);
		  mycpu_unset_alarm(A_MYACIA);
		  fd = -1;
		}
		break;
	}
}

BYTE REGPARM1 read_myacia(ADDRESS a) {
#if 0 /* def DEBUG */
	BYTE read_myacia_(ADDRESS);
	BYTE b = read_myacia_(a);
	static ADDRESS lasta = 0;
	static BYTE lastb = 0;

	if((a!=lasta) || (b!=lastb)) {
	  printf("read_myacia(%04x) -> %02x\n",a,b);
	}
	lasta = a; lastb = b;
	return b;
}
BYTE read_myacia_(ADDRESS a) {
#endif

	switch(a & 3) {
	case ACIA_DR:
		status &= ~8;
		return rxdata;
	case ACIA_SR:
		{
		  BYTE c = status | (irq?0x80:0);
		  mycpu_set_int(I_MYACIA, 0);
		  irq = 0;
		  return c;
		}
	case ACIA_CTRL:
		return ctrl;
	case ACIA_CMD:
		return cmd;
	}
	return 0;
}

BYTE peek_myacia(ADDRESS a) {

	switch(a & 3) {
	case ACIA_DR:
		return rxdata;
	case ACIA_SR:
		{
		  BYTE c = status | (irq?0x80:0);
		  return c;
		}
	case ACIA_CTRL:
		return ctrl;
	case ACIA_CMD:
		return cmd;
	}
	return 0;
}

int int_myacia(long offset) {
#if 0 /*def DEBUG*/
	printf("int_myacia(clk=%ld)\n",myclk-offset);
#endif

	if(intx==2 && fd>=0) rs232_putc(fd,txdata);
	if(intx) intx--;

	if(!(status&0x10)) {
	  status |= 0x10;
	}

        if( fd>=0 && (!(status&8)) && rs232_getc(fd, &rxdata)) {
          status |= 8;
        }

	mycpu_set_int(I_MYACIA, myacia_irq);
	irq = 1;

	mycpu_set_alarm(A_MYACIA, ACIA_TICKS);

	return 0;
}

