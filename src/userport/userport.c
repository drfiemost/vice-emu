/*
 * userport.c - userport handling.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
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

#include "lib.h"
#include "log.h"
#include "resources.h"
#include "userport.h"
#include "util.h"

static int userport_collision_handling = 0;
static unsigned int order = 0;
static int userport_collision_method = USERPORT_COLLISION_METHOD_DETACH_ALL;
static userport_device_list_t userport_head = { NULL, NULL, NULL };
static userport_port_props_t userport_props;

/* ---------------------------------------------------------------------------------------------------------- */

static int valid_device(userport_device_t *device)
{
    if ((device->read_pa2 || device->store_pa2) && !userport_props.has_pa2) {
        return 0;
    }

    if ((device->read_pa3 || device->store_pa3) && !userport_props.has_pa3) {
        return 0;
    }

    if ((device->read_flag || device->store_flag) && !userport_props.has_flag) {
        return 0;
    }

    if (device->read_pc && !userport_props.has_pc) {
        return 0;
    }

    if ((device->cnt12 || device->read_sp) && !userport_props.has_cnt12_sp) {
        return 0;
    }

    return 1;
}

/* ---------------------------------------------------------------------------------------------------------- */

void userport_port_register(userport_port_props_t *props)
{
    userport_props.has_pa2 = props->has_pa2;
    userport_props.has_pa3 = props->has_pa3;
    userport_props.has_flag = props->has_flag;
    userport_props.has_pc = props->has_pc;
    userport_props.has_cnt12_sp = props->has_cnt12_sp;
}

userport_device_list_t *userport_device_register(userport_device_t *device)
{
    userport_device_list_t *current = &userport_head;
    userport_device_list_t *retval = NULL;

    if (valid_device(device)) {
        retval = lib_malloc(sizeof(userport_device_list_t));

        while (current->next != NULL) {
            current = current->next;
        }
        current->next = retval;
        retval->previous = current;
        retval->device = device;
        retval->next = NULL;
        retval->device->order = order++;
    }

    return retval;
}

void userport_device_unregister(userport_device_list_t *device)
{
    userport_device_list_t *prev;

    if (device) {
        prev = device->previous;
        prev->next = device->next;

        if (device->next) {
            device->next->previous = prev;
        }

        if (device->device->order == order - 1) {
            if (order != 0) {
                order--;
            }
        }

        lib_free(device);
    }
}

/* ---------------------------------------------------------------------------------------------------------- */

static void userport_detach_devices(int collision, unsigned int highest_order)
{
    userport_device_list_t *current = userport_head.next;
    char *tmp1 = lib_stralloc("Userport collision detected from ");
    char *tmp2;
    int col_found = 0;
    char *last_device_resource = NULL;
    char *last_device = NULL;
    char **detach_resource_list = NULL;
    int i;

    if (userport_collision_handling == USERPORT_COLLISION_METHOD_DETACH_ALL) {
        detach_resource_list = lib_malloc(sizeof(char *) * (collision + 1));
        memset(detach_resource_list, 0, sizeof(char *) * (collision + 1));
    }

    while (current) {
        if (current->device->collision) {
            if (userport_collision_handling == USERPORT_COLLISION_METHOD_DETACH_ALL) {
                detach_resource_list[col_found] = current->device->resource;
            }
            ++col_found;
            if (current->device->order == highest_order) {
                last_device_resource = current->device->resource;
                last_device = current->device->name;
            }
            if (col_found == collision) {
                tmp2 = util_concat(tmp1, "and ", current->device->name, NULL);
            } else if (col_found == 1) {
                tmp2 = util_concat(tmp1, current->device->name, NULL);
            } else {
                tmp2 = util_concat(tmp1, ", ", current->device->name, NULL);
            }
            lib_free(tmp1);
            tmp1 = tmp2;
        }
        current = current->next;
    }

    if (userport_collision_handling == USERPORT_COLLISION_METHOD_DETACH_ALL) {
        tmp2 = util_concat(tmp1, ". All involved devices will be detached.", NULL);
        for (i = 0; detach_resource_list[i]; ++i) {
            resources_set_int(detach_resource_list[i], 0);
        }
        lib_free(detach_resource_list);
    } else {
        tmp2 = util_concat(tmp1, ". Last device (", last_device, ") will be detached.", NULL);
        resources_set_int(last_device_resource, 0);
    }

    lib_free(tmp1);
    log_message(LOG_DEFAULT, tmp2);
    lib_free(tmp2);
}

static BYTE userport_detect_collision(BYTE retval_orig, BYTE mask)
{
    BYTE retval = retval_orig;
    BYTE rm;
    BYTE rv;
    int collision = 0;
    int first_found = 0;
    userport_device_list_t *current = userport_head.next;
    unsigned int highest_order = 0;

    /* collision detection */
    current = userport_head.next;

    while (current) {
        if (current->device->read_pbx != NULL) {
            rm = current->device->mask;
            rm &= mask;
            if (rm) {
                rv = current->device->retval;
                rv |= ~rm;
                rv = 0xff & rv;
                if (!first_found) {
                    retval = rv;
                    first_found = 1;
                    current->device->collision = 1;
                    if (highest_order < current->device->order) {
                        highest_order = current->device->order;
                    }
                } else {
                    if (rv != retval) {
                        ++collision;
                        current->device->collision = 1;
                        if (highest_order < current->device->order) {
                            highest_order = current->device->order;
                        }
                    }
                }
            }
        }
    }

    if (collision) {
        userport_detach_devices(collision + 1, highest_order);
        if (userport_collision_handling == USERPORT_COLLISION_METHOD_DETACH_ALL) {
            retval = 0xff;
        }
    }

    return retval;
}

BYTE read_userport_pbx(BYTE mask)
{
    BYTE retval = 0xff;
    BYTE rm;
    BYTE rv;
    int valid = 0;
    userport_device_list_t *current = userport_head.next;

    if (!mask) {
        return 0xff;
    }

    /* set retval */
    while (current) {
        current->device->collision = 0;
        if (current->device->read_pbx != NULL) {
            current->device->read_pbx();
            rm = current->device->mask;
            rm &= mask;
            if (rm) {
                rv = current->device->retval;
                rv |= ~rm;
                retval &= rv;
                ++valid;
            }
        }
        current = current->next;
    }

    if (valid > 1 && userport_collision_handling != USERPORT_COLLISION_METHOD_AND_WIRES) {
        return userport_detect_collision(retval, mask);
    }

    return retval;
}

void store_userport_pbx(BYTE val, BYTE mask)
{
    userport_device_list_t *current = userport_head.next;

    while (current) {
        if (current->device->store_pbx != NULL) {
            current->device->store_pbx(val, mask);
        }
        current = current->next;
    }
}

BYTE read_userport_pa2(void)
{
    BYTE mask = 1;
    BYTE rm;
    BYTE rv;
    BYTE retval = 0xff;
    int valid = 0;
    userport_device_list_t *current = userport_head.next;

    /* set retval */
    while (current) {
        current->device->collision = 0;
        if (current->device->read_pa2 != NULL) {
            current->device->read_pa2();
            rm = current->device->mask;
            rm &= mask;
            if (rm) {
                rv = current->device->retval;
                rv |= ~rm;
                retval &= rv;
                ++valid;
            }
        }
        current = current->next;
    }

    if (valid > 1 && userport_collision_handling != USERPORT_COLLISION_METHOD_AND_WIRES) {
        return userport_detect_collision(retval, mask);
    }

    return retval;
}

void store_userport_pa2(BYTE val)
{
    userport_device_list_t *current = userport_head.next;

    while (current) {
        if (current->device->store_pa2 != NULL) {
            current->device->store_pa2(val);
        }
        current = current->next;
    }
}

BYTE read_userport_pa3(void)
{
    BYTE mask = 1;
    BYTE rm;
    BYTE rv;
    BYTE retval = 0xff;
    int valid = 0;
    userport_device_list_t *current = userport_head.next;

    /* set retval */
    while (current) {
        current->device->collision = 0;
        if (current->device->read_pa3 != NULL) {
            current->device->read_pa3();
            rm = current->device->mask;
            rm &= mask;
            if (rm) {
                rv = current->device->retval;
                rv |= ~rm;
                retval &= rv;
                ++valid;
            }
        }
        current = current->next;
    }

    if (valid > 1 && userport_collision_handling != USERPORT_COLLISION_METHOD_AND_WIRES) {
        return userport_detect_collision(retval, mask);
    }

    return retval;
}

void store_userport_pa3(BYTE val)
{
    userport_device_list_t *current = userport_head.next;

    while (current) {
        if (current->device->store_pa3 != NULL) {
            current->device->store_pa3(val);
        }
        current = current->next;
    }
}

BYTE read_userport_flag(void)
{
    BYTE mask = 1;
    BYTE rm;
    BYTE rv;
    BYTE retval = 0xff;
    int valid = 0;
    userport_device_list_t *current = userport_head.next;

    /* set retval */
    while (current) {
        current->device->collision = 0;
        if (current->device->read_flag != NULL) {
            current->device->read_flag();
            rm = current->device->mask;
            rm &= mask;
            if (rm) {
                rv = current->device->retval;
                rv |= ~rm;
                retval &= rv;
                ++valid;
            }
        }
        current = current->next;
    }

    if (valid > 1 && userport_collision_handling != USERPORT_COLLISION_METHOD_AND_WIRES) {
        return userport_detect_collision(retval, mask);
    }

    return retval;
}

void store_userport_flag(BYTE val)
{
    userport_device_list_t *current = userport_head.next;

    while (current) {
        if (current->device->store_flag != NULL) {
            current->device->store_flag(val);
        }
        current = current->next;
    }
}

BYTE read_userport_pc(void)
{
    BYTE mask = 1;
    BYTE rm;
    BYTE rv;
    BYTE retval = 0xff;
    int valid = 0;
    userport_device_list_t *current = userport_head.next;

    /* set retval */
    while (current) {
        current->device->collision = 0;
        if (current->device->read_pc != NULL) {
            current->device->read_pc();
            rm = current->device->mask;
            rm &= mask;
            if (rm) {
                rv = current->device->retval;
                rv |= ~rm;
                retval &= rv;
                ++valid;
            }
        }
        current = current->next;
    }

    if (valid > 1 && userport_collision_handling != USERPORT_COLLISION_METHOD_AND_WIRES) {
        return userport_detect_collision(retval, mask);
    }

    return retval;
}

void userport_cnt12(int enable)
{
    userport_device_list_t *current = userport_head.next;

    while (current) {
        if (current->device->cnt12 != NULL) {
            current->device->cnt12(enable);
        }
        current = current->next;
    }
}

BYTE read_userport_sp(void)
{
    BYTE mask = 1;
    BYTE rm;
    BYTE rv;
    BYTE retval = 0xff;
    int valid = 0;
    userport_device_list_t *current = userport_head.next;

    /* set retval */
    while (current) {
        current->device->collision = 0;
        if (current->device->read_sp != NULL) {
            current->device->read_sp();
            rm = current->device->mask;
            rm &= mask;
            if (rm) {
                rv = current->device->retval;
                rv |= ~rm;
                retval &= rv;
                ++valid;
            }
        }
        current = current->next;
    }

    if (valid > 1 && userport_collision_handling != USERPORT_COLLISION_METHOD_AND_WIRES) {
        return userport_detect_collision(retval, mask);
    }

    return retval;
}
