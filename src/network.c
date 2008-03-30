/*
 * network.c - Connecting emulators via network.
 *
 * Written by
 *  Andreas Matthies <andreas.matthies@gmx.net>
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

#ifdef MINIX_SUPPORT
#define _POSIX_SOURCE
#include <limits.h>
#define PF_INET AF_INET
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_NETWORK

#ifdef AMIGA_SUPPORT
#ifdef AMIGA_MORPHOS
#include <proto/socket.h>
struct Library *SocketBase;
#else
#define __USE_INLINE__
#include <proto/bsdsocket.h>
#endif
#define select(nfds, read_fds, write_fds, except_fds, timeout) \
        WaitSelect(nfds, read_fds, write_fds, except_fds, timeout, NULL)
#endif

#ifdef WIN32
#include <winsock.h>
#ifndef FD_SETSIZE
#define FD_SETSIZE 64 /* just in case mingw or msvc doesn't define it */
#endif
#endif

#ifdef __BEOS__
#include <socket.h>
#include <netdb.h>
#include <byteorder.h>
typedef unsigned int SOCKET;
typedef struct timeval TIMEVAL;
#define PF_INET AF_INET
#define INVALID_SOCKET (SOCKET)(~0)
#endif

#if !defined(WIN32) && !defined(__BEOS__)
#if !defined(HAVE_GETDTABLESIZE) && defined(HAVE_GETRLIMIT)
#include <sys/resource.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifndef __MSDOS__
#include <sys/time.h>
#ifndef AMIGA_SUPPORT
#include <sys/select.h>
#endif
#include <unistd.h>
#endif

typedef unsigned int SOCKET;
typedef struct timeval TIMEVAL;

#ifndef AMIGA_MORPHOS
#define closesocket close
#else
#define closesocket CloseSocket
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (SOCKET)(~0)
#endif

#endif /* UNIX */
#endif /* HAVE_NETWORK */

#include "archdep.h"
#include "event.h"
#include "interrupt.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "maincpu.h"
#include "mos6510.h"
#include "network.h"
#include "resources.h"
#ifdef HAS_TRANSLATION
#include "translate.h"
#endif
#include "types.h"
#include "ui.h"
#include "uiapi.h"
#include "util.h"
#include "vsync.h"
#include "vsyncapi.h"

/* #define NETWORK_DEBUG */

static network_mode_t network_mode = NETWORK_IDLE;

#ifdef HAVE_NETWORK
static int current_send_frame;
static int last_received_frame;
static SOCKET listen_socket;
static SOCKET network_socket;
static fd_set fdsockset;
static int network_init_done = 0;
static int suspended;

static char *server_name = NULL;
static unsigned short server_port;
static int frame_delta;
static unsigned int network_control;

static int frame_buffer_full;
static int current_frame, frame_to_play;
static event_list_state_t *frame_event_list = NULL;
static char *snapshotfilename;

#ifdef HAVE_IPV6
static int netplay_ipv6=0;
#endif

#ifndef HAVE_HTONL
static unsigned int htonl(unsigned int ip)
{
#ifdef WORDS_BIGENDIAN
    return ip;
#else
    unsigned int ip2;

    ip2=((ip>>24)&0xff)+(((ip>>16)&0xff)<<8)+(((ip>>8)&0xff)<<16)+((ip&0xff)<<24);
    return ip2;
#endif
}
#endif

#ifndef HAVE_HTONS
static unsigned short htons(unsigned short ip)
{
#ifdef WORDS_BIGENDIAN
    return ip;
#else
    unsigned short ip2;

    ip2=((ip>>8)&0xff)+((ip&0xff)<<8);
    return ip2;
#endif
}
#endif

static int set_server_name(resource_value_t v, void *param)
{
    util_string_set(&server_name, (const char *)v);
    return 0;
}

static int set_server_port(resource_value_t v, void *param)
{
    CLOCK tmp = (CLOCK)v;

    server_port = (unsigned short) tmp;

    /* make sure no significant bits were lost */
    assert(server_port == tmp);

    return 0;
}

static int set_network_control(resource_value_t v, void *param)
{
    network_control = (unsigned int)v;
    /* don't let the server loose control */
    network_control |= NETWORK_CONTROL_RSRC;

    return 0;
}

static int network_init(void)
{
    if (network_init_done)
        return 0;

#if defined(HAVE_NETWORK) && defined(AMIGA_MORPHOS)
    if (SocketBase == NULL) {
        SocketBase = OpenLibrary("bsdsocket.library", 3);
        if (SocketBase == NULL) {
            return -1;
        }
    }
#endif

    network_mode = NETWORK_IDLE;
    network_init_done = 1;

    return 0;
}

#ifdef HAVE_IPV6
static int set_netplay_ipv6(resource_value_t v, void *param)
{
    if (network_init() < 0)
        return -1;
    if (network_mode!=NETWORK_IDLE) {
#ifdef HAS_TRANSLATION
        ui_error(translate_text(IDGS_CANNOT_SWITCH_IPV4_IPV6));
#else
        ui_error(_("Cannot switch IPV4/IPV6 while netplay is active."));
#endif
        return -1;
    }
    netplay_ipv6 = (int)v;
    return 0;
}
#endif

/*---------- Resources ------------------------------------------------*/

static const resource_t resources[] = {
    { "NetworkServerName", RES_STRING, (resource_value_t)"127.0.0.1",
      RES_EVENT_NO, NULL,
      (void *)&server_name,
      set_server_name, NULL },
    { "NetworkServerPort", RES_INTEGER, (resource_value_t)6502,
      RES_EVENT_NO, NULL,
      (void *)&server_port,
      set_server_port, NULL },
    { "NetworkControl", RES_INTEGER, (resource_value_t)NETWORK_CONTROL_DEFAULT,
      RES_EVENT_SAME, NULL,
      (void *)&network_control,
      set_network_control, NULL },
#ifdef HAVE_IPV6
    { "NetworkIPV6", RES_INTEGER, (resource_value_t)0,
      RES_EVENT_NO, NULL,
      (void *)&netplay_ipv6,
      set_netplay_ipv6, NULL },
#endif
    { NULL }
};
#endif

int network_resources_init(void)
{
#ifdef HAVE_NETWORK
    return resources_register(resources);
#else
    return 0;
#endif
}
/*---------------------------------------------------------------------*/

#ifdef HAVE_NETWORK

static int get_select_fd_size(void)
{
#if !defined(HAVE_GETDTABLESIZE) && !defined(HAVE_GETRLIMIT)
    return FD_SETSIZE;
#endif

#if !defined(HAVE_GETDTABLESIZE) && defined(HAVE_GETRLIMIT)
    struct rlimit size;
    if (!getrlimit(RLIMIT_NOFILE, &size))
        return size.rlim_cur;
    else
        return FD_SETSIZE;
#endif

#if defined(HAVE_GETDTABLESIZE)
    return getdtablesize();
#endif
}

static void network_free_frame_event_list(void)
{
    int i;

    if (frame_event_list != NULL) {
        for (i=0; i < frame_delta; i++)
            event_clear_list(&(frame_event_list[i]));
        lib_free(frame_event_list);
        frame_event_list = NULL;
    }
    event_destroy_image_list();
}

static void network_event_record_sync_test(WORD addr, void *data)
{
    unsigned int regbuf[5];
        
    regbuf[0] = maincpu_regs.pc;
    regbuf[1] = maincpu_regs.a;
    regbuf[2] = maincpu_regs.x;
    regbuf[3] = maincpu_regs.y;
    regbuf[4] = maincpu_regs.sp;

    network_event_record(EVENT_SYNC_TEST, (void *)regbuf, sizeof(regbuf));
}

static void network_init_frame_event_list(void)
{
    frame_event_list = lib_malloc(sizeof(event_list_state_t) * frame_delta);
    memset(frame_event_list, 0, sizeof(event_list_state_t) * frame_delta);
    current_frame = 0;
    frame_buffer_full = 0;
    event_register_event_list(&(frame_event_list[0]));
    event_init_image_list();
    interrupt_maincpu_trigger_trap(network_event_record_sync_test, (void *)0);
}

static void network_prepare_next_frame(void)
{
    current_frame = (current_frame + 1) % frame_delta;
    frame_to_play = (current_frame + 1) % frame_delta;
    event_clear_list(&(frame_event_list[current_frame]));
    event_register_event_list(&(frame_event_list[current_frame]));
    interrupt_maincpu_trigger_trap(network_event_record_sync_test, (void *)0);
}

static unsigned int network_create_event_buffer(BYTE **buf,
                                                event_list_state_t *list)
{
    int size;
    BYTE *bufptr;
    event_list_t *current_event, *last_event;
    int data_len = 0;
    int num_of_events;

    if (list == NULL)
        return 0;

    /* calculate the buffer length */
    num_of_events = 0;
    current_event = list->base;
    do {
        num_of_events++;
        data_len += current_event->size;
        last_event = current_event;
        current_event = current_event->next;
    } while (last_event->type != EVENT_LIST_END);

    size = num_of_events * 3 * sizeof(DWORD) + data_len;
    
    *buf = lib_malloc(size);
    
    /* fill the buffer with the events */
    current_event = list->base;
    bufptr = *buf;
    do {
        util_dword_to_le_buf(&bufptr[0], (DWORD)(current_event->type));
        util_dword_to_le_buf(&bufptr[4], (DWORD)(current_event->clk));
        util_dword_to_le_buf(&bufptr[8], (DWORD)(current_event->size));
        memcpy(&bufptr[12], current_event->data, current_event->size);
        bufptr += 12 + current_event->size;
        last_event = current_event;
        current_event = current_event->next;
    } while (last_event->type != EVENT_LIST_END);

    return size;
}

static event_list_state_t *network_create_event_list(BYTE *remote_event_buffer)
{
    event_list_state_t *list;
    unsigned int type, size;
    CLOCK clk;
    BYTE *data;
    BYTE *bufptr = remote_event_buffer;

    list = lib_malloc(sizeof(event_list_state_t));
    event_register_event_list(list);

    do {
        type = util_le_buf_to_dword(&bufptr[0]);
        clk = util_le_buf_to_dword(&bufptr[4]);
        size = util_le_buf_to_dword(&bufptr[8]);
        data = &bufptr[12];
        bufptr += 12 + size;
        event_record_in_list(list, type, data, size);
    } while (type != EVENT_LIST_END);

    return list;
}

static int network_recv_buffer(SOCKET s, BYTE *buf, int len)
{
    int t;
    int received_total = 0;

    while (received_total < len) {
        t = recv(s, buf, len - received_total, 0);
        
        if (t < 0)
            return t;

        received_total += t;
        buf += t;
    }
    return 0;
}

static int network_send_buffer(SOCKET s, const BYTE *buf, int len)
{
    int t;
    int sent_total = 0;

    while (sent_total < len) {
        t = send(s, buf, len - sent_total, 0);
        
        if (t < 0)
            return t;

        sent_total += t;
        buf += t;
    }
    return 0;
}

#define NUM_OF_TESTPACKETS 50

static void network_test_delay(void)
{
    int i, j;
    BYTE new_frame_delta;
    BYTE buf[0x60];
    long packet_delay[NUM_OF_TESTPACKETS];
    char st[256];

    vsyncarch_init();

#ifdef HAS_TRANSLATION
    ui_display_statustext(translate_text(IDGS_TESTING_BEST_FRAME_DELAY), 0);
#else
    ui_display_statustext(_("Testing best frame delay..."), 0);
#endif

    if (network_mode == NETWORK_SERVER_CONNECTED) {
        for (i = 0; i < NUM_OF_TESTPACKETS; i++) {
            *((unsigned long*)buf) = vsyncarch_gettime();
            if (network_send_buffer(network_socket, buf, sizeof(buf)) <  0
                || network_recv_buffer(network_socket, buf, sizeof(buf)) <  0)
                return;
            packet_delay[i] = vsyncarch_gettime() - *((unsigned long*)buf);
        }
        /* Sort the packets delays*/
        for (i = 0; i < NUM_OF_TESTPACKETS - 1; i++) {
            for (j = i + 1; j < NUM_OF_TESTPACKETS; j++) {
                if (packet_delay[i] < packet_delay[j]) {
                    long d = packet_delay[i];
                    packet_delay[i] = packet_delay[j];
                    packet_delay[j] = d;
                }
            }
#ifdef NETWORK_DEBUG
            log_debug("packet_delay[%d]=%d",i,packet_delay[i]);
#endif
        }
#ifdef NETWORK_DEBUG
        log_debug("vsyncarch_frequency = %d", vsyncarch_frequency());
#endif
        /* calculate delay with 90% of packets beeing fast enough */
        /* FIXME: This needs some further investigation */
        new_frame_delta = 5 + (BYTE)(vsync_get_refresh_frequency()
                            * packet_delay[(int)(0.1 * NUM_OF_TESTPACKETS)]
                            / (float)vsyncarch_frequency());
        network_send_buffer(network_socket, &new_frame_delta, sizeof(new_frame_delta));
    } else {
        /* network_mode == NETWORK_CLIENT */
        for (i = 0; i < NUM_OF_TESTPACKETS; i++) {
            if (network_recv_buffer(network_socket, buf, sizeof(buf)) <  0
                || network_send_buffer(network_socket, buf, sizeof(buf)) < 0)
                return;
        }
        network_recv_buffer(network_socket, &new_frame_delta, sizeof(new_frame_delta));
    }
    network_free_frame_event_list();
    frame_delta = new_frame_delta;
    network_init_frame_event_list();
#ifdef HAS_TRANSLATION
    sprintf(st, translate_text(IDGS_USING_D_FRAMES_DELAY), frame_delta);
#else
    sprintf(st, _("Using %d frames delay."), frame_delta);
#endif
    log_debug("netplay connected with %d frames delta.", frame_delta);
    ui_display_statustext(st, 1);
}

static void network_server_connect_trap(WORD addr, void *data)
{
    FILE *f;
    BYTE *buf;
    long buf_size;
    long i;
    event_list_state_t settings_list;

    vsync_suspend_speed_eval();

    /* Create snapshot and send it */
    snapshotfilename = archdep_tmpnam();
    if (machine_write_snapshot(snapshotfilename, 1, 1, 0) == 0) {
        f = fopen(snapshotfilename, MODE_READ);
        if (f == NULL) {
#ifdef HAS_TRANSLATION
            ui_error(translate_text(IDGS_CANNOT_LOAD_SNAPSHOT_TRANSFER));
#else
            ui_error(_("Cannot load snapshot file for transfer"));
#endif
            lib_free(snapshotfilename);
            return;
        }
        buf_size = util_file_length(f);
        buf = lib_malloc(buf_size);
        fread(buf, 1, buf_size, f);
        fclose(f);

#ifdef HAS_TRANSLATION
        ui_display_statustext(translate_text(IDGS_SENDING_SNAPSHOT_TO_CLIENT), 0);
#else
        ui_display_statustext(_("Sending snapshot to client..."), 0);
#endif
        network_send_buffer(network_socket, (BYTE*)&buf_size, sizeof(long));
        i = network_send_buffer(network_socket, buf, buf_size);
        lib_free(buf);
        if (i < 0) {
#ifdef HAS_TRANSLATION
            ui_error(translate_text(IDGS_CANNOT_SEND_SNAPSHOT_TO_CLIENT));
#else
            ui_error(_("Cannot send snapshot to client"));
#endif
            ui_display_statustext("", 0);
            lib_free(snapshotfilename);
            return;
        }

        network_mode = NETWORK_SERVER_CONNECTED;

        /* Send settings that need to be the same */
        event_register_event_list(&settings_list);
        resources_get_event_safe_list(&settings_list);
        buf_size = network_create_event_buffer(&buf, &(settings_list));
        network_send_buffer(network_socket, (BYTE*)&buf_size, sizeof(long));
        network_send_buffer(network_socket, buf, buf_size);
        event_clear_list(&settings_list);
        lib_free(buf);

        current_send_frame = 0;
        last_received_frame = 0;

        network_test_delay();
    } else {
#ifdef HAS_TRANSLATION
        ui_error(translate_text(IDGS_CANNOT_CREATE_SNAPSHOT_FILE_S), snapshotfilename);
#else
        ui_error(_("Cannot create snapshot file %s"), snapshotfilename);
#endif
    }
    lib_free(snapshotfilename);
}

static void network_client_connect_trap(WORD addr, void *data)
{
    BYTE *buf;
    long buf_size;
    event_list_state_t *settings_list;

    /* Set proper settings */
    if (resources_set_event_safe() < 0)
        ui_error("Warning! Failed to set netplay-safe settings.");

    /* Receive settings that need to be same as on server */
    if (network_recv_buffer(network_socket, (BYTE*)&buf_size, sizeof(long)) < 0)
        return;

    buf = lib_malloc(buf_size);

    if (network_recv_buffer(network_socket, buf, buf_size) < 0)
        return;

    settings_list = network_create_event_list(buf);
    lib_free(buf);

    event_playback_event_list(settings_list);

    event_clear_list(settings_list);
    lib_free(settings_list);

    /* read the snapshot */
    if (machine_read_snapshot(snapshotfilename, 0) != 0) {
#ifdef HAS_TRANSLATION
        ui_error(translate_text(IDGS_CANNOT_OPEN_SNAPSHOT_FILE_S), snapshotfilename);
#else
        ui_error(_("Cannot open snapshot file %s"), snapshotfilename);
#endif
        lib_free(snapshotfilename);
        return;
    }

    current_send_frame = 0;
    last_received_frame = 0;

    network_mode = NETWORK_CLIENT;

    network_test_delay();
    lib_free(snapshotfilename);
}
#endif
/*-------------------------------------------------------------------------*/

void network_event_record(unsigned int type, void *data, unsigned int size)
{
#ifdef HAVE_NETWORK
    unsigned int control = 0;
    BYTE joyport;

    switch (type) {
      case EVENT_KEYBOARD_MATRIX:
      case EVENT_KEYBOARD_RESTORE:
      case EVENT_KEYBOARD_DELAY:
      case EVENT_KEYBOARD_CLEAR:
          control = NETWORK_CONTROL_KEYB;
          break;
      case EVENT_ATTACHDISK:
      case EVENT_ATTACHTAPE:
      case EVENT_DATASETTE:
          control = NETWORK_CONTROL_DEVC;
          break;
      case EVENT_RESOURCE:
      case EVENT_RESETCPU:
          control = NETWORK_CONTROL_RSRC;
          break;
      case EVENT_JOYSTICK_VALUE:
          joyport = ((BYTE*)data)[0];
          if (joyport == 1) 
              control = NETWORK_CONTROL_JOY1;
          if (joyport == 2) 
              control = NETWORK_CONTROL_JOY2;
          break;
      default:
          control = 0;
    }

    if (network_get_mode() == NETWORK_CLIENT)
        control <<= NETWORK_CONTROL_CLIENTOFFSET;

    if (control != 0 && (control & network_control) == 0)
        return;

    event_record_in_list(&(frame_event_list[current_frame]), type, data, size);
#endif
}

void network_attach_image(unsigned int unit, const char *filename)
{
#ifdef HAVE_NETWORK
    unsigned int control = NETWORK_CONTROL_DEVC;

    if (network_get_mode() == NETWORK_CLIENT)
        control <<= NETWORK_CONTROL_CLIENTOFFSET;

    if ((control & network_control) == 0)
        return;

    event_record_attach_in_list(&(frame_event_list[current_frame]), unit, filename, 1);
#endif
}

int network_get_mode(void)
{
    return network_mode;
}

int network_connected(void)
{
#ifdef HAVE_NETWORK
    if (network_mode == NETWORK_SERVER_CONNECTED 
        || network_mode ==  NETWORK_CLIENT)
        return 1;
    else
        return 0;
#else
    return 0;
#endif
}

int network_start_server(void)
{
#ifdef HAVE_NETWORK
#ifdef HAVE_IPV6
    struct sockaddr_in6 server_addr6;
#endif
    int return_value;
    struct sockaddr_in server_addr;

    if (network_init() < 0)
        return -1;

    if (network_mode != NETWORK_IDLE)
        return -1;

#ifdef HAVE_IPV6
    if (netplay_ipv6) {
        bzero((char*)&server_addr6, sizeof(struct sockaddr_in6));
        server_addr6.sin6_port = htons(server_port);
        server_addr6.sin6_family = PF_INET6;
        server_addr6.sin6_addr=in6addr_any;
    } else {
#endif
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = htonl(0);
    server_addr.sin_family = PF_INET;
#ifndef MINIX_SUPPORT
    memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));
#endif
#ifdef HAVE_IPV6
    }
    if (netplay_ipv6)
        listen_socket = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
    else
#endif
    listen_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
        return -1;

#ifdef HAVE_IPV6
    if (netplay_ipv6)
        return_value=bind(listen_socket, (struct sockaddr *)&server_addr6, sizeof(server_addr6));
    else
#endif
    return_value=bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (return_value < 0) {
        closesocket(listen_socket);
        return -1;
    }

    if (listen(listen_socket, 2) < 0) {
        closesocket(listen_socket);
        return -1;
    }

    /* Set proper settings */
    if (resources_set_event_safe() < 0)
        ui_error("Warning! Failed to set netplay-safe settings.");

    network_mode = NETWORK_SERVER;

    vsync_suspend_speed_eval();
#ifdef HAS_TRANSLATION
    ui_display_statustext(translate_text(IDGS_SERVER_IS_WAITING_FOR_CLIENT), 1);
#else
    ui_display_statustext(_("Server is waiting for a client..."), 1);
#endif
#endif
    return 0;
} 


int network_connect_client(void)
{
#ifdef HAVE_NETWORK
    struct sockaddr_in server_addr;
#ifdef HAVE_IPV6
    struct sockaddr_in6 server_addr6;
#ifndef HAVE_GETHOSTBYNAME2
    int err6;
#endif
#endif
    struct hostent *server_hostent;
    FILE *f;
    BYTE *buf;
    long buf_size;
    int return_value;

    if (network_init() < 0)
        return -1;

    if (network_mode != NETWORK_IDLE)
        return -1;

    vsync_suspend_speed_eval();

    snapshotfilename = archdep_tmpnam();
    f = fopen(snapshotfilename, MODE_WRITE);
    if (f == NULL) {
#ifdef HAS_TRANSLATION
        ui_error(translate_text(IDGS_CANNOT_CREATE_SNAPSHOT_S_SELECT), snapshotfilename);
#else
        ui_error(_("Cannot create snapshot file %s. Select different history directory!"), snapshotfilename);
#endif
        lib_free(snapshotfilename);
        return -1;
    }

#ifdef HAVE_IPV6
    if (netplay_ipv6)
#ifdef HAVE_GETHOSTBYNAME2
        server_hostent = gethostbyname2(server_name, PF_INET6);
#else
        server_hostent = getipnodebyname(server_name, PF_INET6, AI_DEFAULT, &err6);
#endif
    else
#endif
    server_hostent = gethostbyname(server_name);
    if (server_hostent == NULL) {
#ifdef HAS_TRANSLATION
        ui_error(translate_text(IDGS_CANNOT_RESOLVE_S), server_name);
#else
        ui_error(_("Cannot resolve %s"), server_name);
#endif
        return -1;
    }
#ifdef HAVE_IPV6
    if (netplay_ipv6) {
        bzero((char*)&server_addr6, sizeof(struct sockaddr_in6));
        server_addr6.sin6_port = htons(server_port);
        server_addr6.sin6_family = PF_INET6;
        memcpy(&server_addr6.sin6_addr, server_hostent->h_addr, server_hostent->h_length);
    } else {
#endif
    server_addr.sin_port = htons(server_port);
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr = *(struct in_addr *)server_hostent->h_addr_list[0];
#ifdef HAVE_IPV6
    }
    if (netplay_ipv6)
        network_socket = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
    else
#endif
    network_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (network_socket == INVALID_SOCKET) {
        lib_free(snapshotfilename);
#if defined(HAVE_IPV6) && !defined(HAVE_GETHOSTBYNAME2)
        if (netplay_ipv6)
            freehostent(server_hostent);
#endif
        return -1;
    }

#ifdef HAVE_IPV6
    if (netplay_ipv6)
        return_value=connect(network_socket, (struct sockaddr *)&server_addr6,
                             sizeof(server_addr6));
    else
#endif
    return_value=connect(network_socket, (struct sockaddr *)&server_addr, 
        sizeof(server_addr));
    if (return_value < 0) {
        closesocket(network_socket);
#ifdef HAS_TRANSLATION
        ui_error(translate_text(IDGS_CANNOT_CONNECT_TO_S),
                    server_name, server_port);
#else
        ui_error(_("Cannot connect to %s (no server running on port %d)."),
                    server_name, server_port);
#endif
        lib_free(snapshotfilename);
#if defined(HAVE_IPV6) && !defined(HAVE_GETHOSTBYNAME2)
        if (netplay_ipv6)
            freehostent(server_hostent);
#endif
        return -1;
    }

#ifdef HAS_TRANSLATION
    ui_display_statustext(translate_text(IDGS_RECEIVING_SNAPSHOT_SERVER), 0);
#else
    ui_display_statustext(_("Receiving snapshot from server..."), 0);
#endif
    if (network_recv_buffer(network_socket, (BYTE*)&buf_size, 
sizeof(long)) < 0)
    {
        lib_free(snapshotfilename);
        closesocket(network_socket);
#if defined(HAVE_IPV6) && !defined(HAVE_GETHOSTBYNAME2)
        if (netplay_ipv6)
            freehostent(server_hostent);
#endif
        return -1;
    }

    buf = lib_malloc(buf_size);

    if (network_recv_buffer(network_socket, buf, buf_size) < 0) {
        lib_free(snapshotfilename);
        closesocket(network_socket);
#if defined(HAVE_IPV6) && !defined(HAVE_GETHOSTBYNAME2)
        if (netplay_ipv6)
            freehostent(server_hostent);
#endif
        return -1;
    }

    fwrite(buf, 1, buf_size, f);
    fclose(f);
    lib_free(buf);
#if defined(HAVE_IPV6) && !defined(HAVE_GETHOSTBYNAME2)
    if (netplay_ipv6)
        freehostent(server_hostent);
#endif

    interrupt_maincpu_trigger_trap(network_client_connect_trap, (void *)0);
    vsync_suspend_speed_eval();
#endif
    return 0;
}

void network_disconnect(void)
{
#ifdef HAVE_NETWORK
    closesocket(network_socket);
    if (network_mode == NETWORK_SERVER_CONNECTED) {
        network_mode = NETWORK_SERVER;
    } else {
        closesocket(listen_socket);
        network_mode = NETWORK_IDLE;
    }
#endif
}

void network_suspend(void)
{
#ifdef HAVE_NETWORK
    int dummy_buf_len = 0;

    if (!network_connected() || suspended == 1)
        return;

    network_send_buffer(network_socket, (BYTE*)&dummy_buf_len, 
                        sizeof(unsigned int));
    
    suspended = 1;
#endif
}

void network_hook(void)
{
#ifdef HAVE_NETWORK
#ifdef NETWORK_DEBUG
    long t1, t2, t3, t4;
#endif

    TIMEVAL time_out = {0, 0};
    int fd_size;

    if (network_mode == NETWORK_IDLE)
        return;

    if (network_mode == NETWORK_SERVER) {
        FD_ZERO(&fdsockset);
        FD_SET(listen_socket, &fdsockset);

        fd_size=get_select_fd_size();
        if (fd_size > FD_SETSIZE)
            fd_size=FD_SETSIZE;

        if (select(fd_size, &fdsockset, NULL, NULL, &time_out) != 0) {
            network_socket = accept(listen_socket, NULL, NULL);
        
            if (network_socket != INVALID_SOCKET)
                interrupt_maincpu_trigger_trap(network_server_connect_trap,
                                               (void *)0);
        }
    }

    if (network_connected()) {
        BYTE *local_event_buf = NULL, *remote_event_buf = NULL;
        unsigned int local_buf_len, remote_buf_len;
        event_list_state_t *remote_event_list;
        event_list_state_t *client_event_list, *server_event_list;

        suspended = 0;

        /* create and send current event buffer */
        network_event_record(EVENT_LIST_END, NULL, 0);
        local_buf_len = network_create_event_buffer(&local_event_buf, &(frame_event_list[current_frame]));
#ifdef NETWORK_DEBUG
        t1 = vsyncarch_gettime();
#endif
        network_send_buffer(network_socket, (BYTE*)&local_buf_len, sizeof(unsigned int));
        network_send_buffer(network_socket, local_event_buf, local_buf_len);
#ifdef NETWORK_DEBUG
        t2 = vsyncarch_gettime();
#endif
        lib_free(local_event_buf);

        /* receive event buffer */
        if (current_frame == frame_delta - 1)
            frame_buffer_full = 1;

        if (frame_buffer_full) {
            do {
                if (network_recv_buffer(network_socket, (BYTE*)&remote_buf_len,
                                        sizeof(unsigned int)) < 0)
                {
#ifdef HAS_TRANSLATION
                    ui_display_statustext(translate_text(IDGS_REMOTE_HOST_DISCONNECTED), 1);
#else
                    ui_display_statustext(_("Remote host disconnected."), 1);
#endif
                    network_disconnect();
                    return;
                }

                if (remote_buf_len == 0 && suspended == 0) {
                    /* remote host suspended emulation */
#ifdef HAS_TRANSLATION
                    ui_display_statustext(translate_text(IDGS_REMOTE_HOST_SUSPENDING), 0);
#else
                    ui_display_statustext(_("Remote host suspending..."), 0);
#endif
                    suspended = 1;
                    vsync_suspend_speed_eval();
                }
            } while(remote_buf_len == 0);

            if (suspended == 1)
                ui_display_statustext("", 0);

            remote_event_buf = lib_malloc(remote_buf_len);

            if (network_recv_buffer(network_socket, remote_event_buf,
                                    remote_buf_len) < 0)
                return;
#ifdef NETWORK_DEBUG
            t3 = vsyncarch_gettime();
#endif
            remote_event_list = network_create_event_list(remote_event_buf);
            lib_free(remote_event_buf);

            if (network_mode == NETWORK_SERVER_CONNECTED) {
                client_event_list = remote_event_list;
                server_event_list = &(frame_event_list[frame_to_play]);
            } else {
                server_event_list = remote_event_list;
                client_event_list = &(frame_event_list[frame_to_play]);
            }

            /* test for sync */
            if (client_event_list->base->type == EVENT_SYNC_TEST
                && server_event_list->base->type == EVENT_SYNC_TEST)
            {
                int i;
                
                for (i = 0; i < 5; i++)
                    if (((unsigned int*)client_event_list->base->data)[i]
                        != ((unsigned int*)server_event_list->base->data)[i])
                    {
#ifdef HAS_TRANSLATION
                        ui_error(translate_text(IDGS_NETWORK_OUT_OF_SYNC));
#else
                        ui_error(_("Network out of sync - disconnecting."));
#endif
                        network_disconnect();
                        /* shouldn't happen but resyncing would be nicer */
                        break;
                    }
            }

            /* replay the event_lists; server first, then client */
            event_playback_event_list(server_event_list);
            event_playback_event_list(client_event_list);

            event_clear_list(remote_event_list);
            lib_free(remote_event_list);
        }
        network_prepare_next_frame();
#ifdef NETWORK_DEBUG
        t4 = vsyncarch_gettime();
        log_debug("network_hook timing: %5d %5d %5d; total: %5d",
                  t2-t1, t3-t2, t4-t3, t4-t1);
#endif
    }
#endif
}

void network_shutdown(void)
{
#ifdef HAVE_NETWORK
	if (network_connected())
		network_disconnect();

    network_free_frame_event_list();
    lib_free(server_name);

#ifdef AMIGA_MORPHOS
    if (SocketBase != NULL) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
    }
#endif
#endif
}
