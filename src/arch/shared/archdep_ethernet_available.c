/** \file   archdep_ethernet_available.c
 * \brief   Determine if ethernet support (libpcap) will actually work
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

/*
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
#include "config.h"
#include "archdep_defs.h"

#include <stdbool.h>
#include <stdio.h>

#ifdef ARCHDEP_OS_UNIX
# include <unistd.h>
# include <sys/types.h>
#elif defined(ARCHDEP_OS_WINDOWS)
# include <windows.h>
#endif

#include "archdep_ethernet_available.h"


/** \brief  Determine if ethernet support is available for the current process
 *
 * On Unix, ethernet support is available via TUN/TAP virtual network devices.
 * If TUN/TAP is not available, then we use libpcap - hence, this function
 * checks that the effective UID is root, since libpcap only works while
 * having root privileges. On Windows it checks for the DLL being loaded.
 * MacOS is currently heaped together with UNIX; a TUN/TAP driver is available,
 * but I don't have a clue how pcap works on MacOS, nor if it is even avaiable.
 *
 * \return  bool
 */
bool archdep_ethernet_available(void)
{
#ifdef ARCHDEP_OS_UNIX
# ifdef HAVE_TUNTAP
    /* When TUN/TAP is available, ethernet support is available for all users */
    return true;
# elif defined HAVE_PCAP
    /* On Linux pcap will only work with root, so we check the EUID for root */
    return geteuid() == 0;
# else
    return false;
# endif
#elif defined ARCHDEP_OS_WINDOWS
    return GetModuleHandleA("WPCAP.DLL") != NULL;
#else
    return false;
#endif
}

