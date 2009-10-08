/*
 * intl.c - Localization routines for Win32.
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

#include "archdep.h"
#include "cmdline.h"
#include "intl.h"
#include "lib.h"
#include "res.h"
#include "resources.h"
#include "translate.h"
#include "util.h"
#include "ui.h"
#include "winmain.h"

#define countof(array) (sizeof(array) / sizeof((array)[0]))

typedef struct windows_iso_s {
    int windows_code;
    char *iso_language_code;
    int code_page;
} windows_iso_t;

/* this table holds only the currently present translation
   languages, to add a new translation look at
   http://www.unicode.org/unicode/onlinedat/languages.html
   for the corresponding windows and iso codes */

static windows_iso_t windows_to_iso[] = {
    { LANG_ENGLISH, "en", 28591 },
    { LANG_DANISH, "da", 28591 },
    { LANG_GERMAN, "de", 28591 },
    { LANG_FRENCH, "fr", 28591 },
    { LANG_HUNGARIAN, "hu", 28592 },
    { LANG_ITALIAN, "it", 28591 },
    { LANG_DUTCH, "nl", 28591 },
    { LANG_POLISH, "pl", 28592 },
    { LANG_SWEDISH, "sv", 28591 },
    { LANG_TURKISH, "tr", 28599 },
    { 0, NULL }
};

/* The language table is duplicated in
   the translate.c, make sure they match
   when adding a new language */

static char *language_table[] = {

/* english */
    "en",

/* danish */
    "da",

/* german */
    "de",

/* french */
    "fr",

/* hungarian */
    "hu",

/* italian */
    "it",

/* dutch */
    "nl",

/* polish */
    "pl",

/* swedish */
    "sv",

/* turkish */
    "tr"
};

static int intl_table[][countof(language_table)] = {

/* ------------------------------ DIALOG RESOURCES ------------------------------ */ 

/* resc64dtv.rc */
/* en */ { IDD_C64DTV_SETTINGS_DIALOG,
/* da */   IDD_C64DTV_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C64DTV_SETTINGS_DIALOG_DE,    /* fuzzy, size */
/* fr */   IDD_C64DTV_SETTINGS_DIALOG_FR,    /* fuzzy, size */
/* hu */   IDD_C64DTV_SETTINGS_DIALOG_HU,    /* fuzzy, size */
/* it */   IDD_C64DTV_SETTINGS_DIALOG_IT,    /* fuzzy, size */
/* nl */   IDD_C64DTV_SETTINGS_DIALOG_NL,
/* pl */   IDD_C64DTV_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C64DTV_SETTINGS_DIALOG_SV,    /* fuzzy, size */
/* tr */   IDD_C64DTV_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resc64dtv.rc */
/* en */ { IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG,
/* da */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_DE,    /* fuzzy, size */
/* fr */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_FR,    /* fuzzy, size */
/* hu */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_HU,    /* fuzzy, size */
/* it */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_IT,    /* fuzzy, size */
/* nl */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_NL,
/* pl */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_SV,    /* fuzzy, size */
/* tr */   IDD_C64DTV_ATTACH_FLASH_IMAGE_DIALOG_TR },  /* fuzzy, size */

/* resc64dtv.rc */
/* en */ { IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG,
/* da */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_DE,    /* fuzzy, size */
/* fr */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_FR,    /* fuzzy, size */
/* hu */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_HU,    /* fuzzy, size */
/* it */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_IT,    /* fuzzy, size */
/* nl */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_NL,
/* pl */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_SV,    /* fuzzy, size */
/* tr */   IDD_C64DTV_CREATE_FLASH_IMAGE_DIALOG_TR },  /* fuzzy, size */

/* resc128.rc */
/* en */ { IDD_C128ROM_RESOURCE_DIALOG,
/* da */   IDD_C128ROM_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C128ROM_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_C128ROM_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_C128ROM_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_C128ROM_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_C128ROM_RESOURCE_DIALOG_NL,
/* pl */   IDD_C128ROM_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C128ROM_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_C128ROM_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* resc128.rc */
/* en */ { IDD_C128ROMDRIVE_RESOURCE_DIALOG,
/* da */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_NL,
/* pl */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_C128ROMDRIVE_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* resc128.rc */
/* en */ { IDD_C128_MACHINE_SETTINGS_DIALOG,
/* da */   IDD_C128_MACHINE_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C128_MACHINE_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_C128_MACHINE_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_C128_MACHINE_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_C128_MACHINE_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_C128_MACHINE_SETTINGS_DIALOG_NL,
/* pl */   IDD_C128_MACHINE_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C128_MACHINE_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_C128_MACHINE_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resc128.rc */
/* en */ { IDD_C128_FUNCTIONROM_SETTINGS_DIALOG,
/* da */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_NL,
/* pl */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_C128_FUNCTIONROM_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resc128.rc */
/* en */ { IDD_C128_RAM_BANKS_DIALOG,
/* da */   IDD_C128_RAM_BANKS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C128_RAM_BANKS_DIALOG_DE,    /* fuzzy, size */
/* fr */   IDD_C128_RAM_BANKS_DIALOG_FR,    /* fuzzy, size */
/* hu */   IDD_C128_RAM_BANKS_DIALOG_HU,    /* fuzzy, size */
/* it */   IDD_C128_RAM_BANKS_DIALOG_IT,    /* fuzzy, size */
/* nl */   IDD_C128_RAM_BANKS_DIALOG_NL,
/* pl */   IDD_C128_RAM_BANKS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C128_RAM_BANKS_DIALOG_SV,    /* fuzzy, size */
/* tr */   IDD_C128_RAM_BANKS_DIALOG_TR },  /* fuzzy, size */

/* resdrivec128.rc */
/* en */ { IDD_DRIVE_SETTINGS_DIALOG_C128,
/* da */   IDD_DRIVE_SETTINGS_DIALOG_C128_DA,    /* fuzzy, size */
/* de */   IDD_DRIVE_SETTINGS_DIALOG_C128_DE,    /* size */
/* fr */   IDD_DRIVE_SETTINGS_DIALOG_C128_FR,    /* size */
/* hu */   IDD_DRIVE_SETTINGS_DIALOG_C128_HU,    /* size */
/* it */   IDD_DRIVE_SETTINGS_DIALOG_C128_IT,    /* size */
/* nl */   IDD_DRIVE_SETTINGS_DIALOG_C128_NL,
/* pl */   IDD_DRIVE_SETTINGS_DIALOG_C128_PL,    /* fuzzy, size */
/* sv */   IDD_DRIVE_SETTINGS_DIALOG_C128_SV,    /* size */
/* tr */   IDD_DRIVE_SETTINGS_DIALOG_C128_TR },  /* fuzzy, size */

/* resc128.rc */
/* en */ { IDD_C128KBD_MAPPING_SETTINGS_DIALOG,
/* da */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_NL,
/* pl */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_C128KBD_MAPPING_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resdrivec64.rc */
/* en */ { IDD_DRIVE_SETTINGS_DIALOG_C64,
/* da */   IDD_DRIVE_SETTINGS_DIALOG_C64_DA,    /* fuzzy, size */
/* de */   IDD_DRIVE_SETTINGS_DIALOG_C64_DE,    /* size */
/* fr */   IDD_DRIVE_SETTINGS_DIALOG_C64_FR,    /* size */
/* hu */   IDD_DRIVE_SETTINGS_DIALOG_C64_HU,    /* size */
/* it */   IDD_DRIVE_SETTINGS_DIALOG_C64_IT,    /* size */
/* nl */   IDD_DRIVE_SETTINGS_DIALOG_C64_NL,
/* pl */   IDD_DRIVE_SETTINGS_DIALOG_C64_PL,    /* fuzzy, size */
/* sv */   IDD_DRIVE_SETTINGS_DIALOG_C64_SV,    /* size */
/* tr */   IDD_DRIVE_SETTINGS_DIALOG_C64_TR },  /* fuzzy, size */

/* resdrivepetcbm2.rc */
/* en */ { IDD_DRIVE_SETTINGS_DIALOG_PETCBM2,
/* da */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_DA,    /* fuzzy, size */
/* de */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_DE,    /* size */
/* fr */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_FR,    /* size */
/* hu */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_HU,    /* size */
/* it */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_IT,    /* size */
/* nl */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_NL,
/* pl */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_PL,    /* fuzzy, size */
/* sv */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_SV,    /* size */
/* tr */   IDD_DRIVE_SETTINGS_DIALOG_PETCBM2_TR },  /* fuzzy, size */

/* resdrivevic20.rc */
/* en */ { IDD_DRIVE_SETTINGS_DIALOG_VIC20,
/* da */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_DA,    /* fuzzy, size */
/* de */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_DE,    /* size */
/* fr */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_FR,    /* size */
/* hu */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_HU,    /* size */
/* it */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_IT,    /* size */
/* nl */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_NL,
/* pl */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_PL,    /* fuzzy, size */
/* sv */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_SV,    /* size */
/* tr */   IDD_DRIVE_SETTINGS_DIALOG_VIC20_PL },  /* fuzzy, size */

/* resgeoram.rc */
/* en */ { IDD_GEORAM_SETTINGS_DIALOG,
/* da */   IDD_GEORAM_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_GEORAM_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_GEORAM_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_GEORAM_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_GEORAM_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_GEORAM_SETTINGS_DIALOG_NL,
/* pl */   IDD_GEORAM_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_GEORAM_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_GEORAM_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* reseasyflash.rc */
/* en */ { IDD_EASYFLASH_SETTINGS_DIALOG,
/* da */   IDD_EASYFLASH_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_EASYFLASH_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_EASYFLASH_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_EASYFLASH_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_EASYFLASH_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_EASYFLASH_SETTINGS_DIALOG_NL,
/* pl */   IDD_EASYFLASH_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_EASYFLASH_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_EASYFLASH_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resisepic.rc */
/* en */ { IDD_ISEPIC_SETTINGS_DIALOG,
/* da */   IDD_ISEPIC_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_ISEPIC_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_ISEPIC_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_ISEPIC_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_ISEPIC_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_ISEPIC_SETTINGS_DIALOG_NL,
/* pl */   IDD_ISEPIC_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_ISEPIC_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_ISEPIC_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resdqbb.rc */
/* en */ { IDD_DQBB_SETTINGS_DIALOG,
/* da */   IDD_DQBB_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_DQBB_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_DQBB_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_DQBB_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_DQBB_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_DQBB_SETTINGS_DIALOG_NL,
/* pl */   IDD_DQBB_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_DQBB_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_DQBB_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resc64.rc */
/* en */ { IDD_C64ROM_RESOURCE_DIALOG,
/* da */   IDD_C64ROM_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C64ROM_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_C64ROM_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_C64ROM_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_C64ROM_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_C64ROM_RESOURCE_DIALOG_NL,
/* pl */   IDD_C64ROM_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C64ROM_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_C64ROM_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* resc64.rc */
/* en */ { IDD_C64ROMDRIVE_RESOURCE_DIALOG,
/* da */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_NL,
/* pl */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_C64ROMDRIVE_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* resc64.rc */
/* en */ { IDD_C64KBD_MAPPING_SETTINGS_DIALOG,
/* da */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_NL,
/* pl */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_C64KBD_MAPPING_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* rescbm2.rc */
/* en */ { IDD_CBM2ROM_RESOURCE_DIALOG,
/* da */   IDD_CBM2ROM_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_CBM2ROM_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_CBM2ROM_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_CBM2ROM_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_CBM2ROM_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_CBM2ROM_RESOURCE_DIALOG_NL,
/* pl */   IDD_CBM2ROM_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_CBM2ROM_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_CBM2ROM_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* rescbm2.rc */
/* en */ { IDD_CBM2ROMDRIVE_RESOURCE_DIALOG,
/* da */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_NL,
/* pl */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_CBM2ROMDRIVE_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* rescbm2.rc */
/* en */ { IDD_CBMII_SETTINGS_MODEL_DIALOG,
/* da */   IDD_CBMII_SETTINGS_MODEL_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_CBMII_SETTINGS_MODEL_DIALOG_DE,    /* size */
/* fr */   IDD_CBMII_SETTINGS_MODEL_DIALOG_FR,    /* size */
/* hu */   IDD_CBMII_SETTINGS_MODEL_DIALOG_HU,    /* size */
/* it */   IDD_CBMII_SETTINGS_MODEL_DIALOG_IT,    /* size */
/* nl */   IDD_CBMII_SETTINGS_MODEL_DIALOG_NL,
/* pl */   IDD_CBMII_SETTINGS_MODEL_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_CBMII_SETTINGS_MODEL_DIALOG_SV,    /* size */
/* tr */   IDD_CBMII_SETTINGS_MODEL_DIALOG_TR },  /* fuzzy, size */

/* rescbm2.rc */
/* en */ { IDD_CBMII_SETTINGS_IO_DIALOG,
/* da */   IDD_CBMII_SETTINGS_IO_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_CBMII_SETTINGS_IO_DIALOG_DE,    /* size */
/* fr */   IDD_CBMII_SETTINGS_IO_DIALOG_FR,    /* size */
/* hu */   IDD_CBMII_SETTINGS_IO_DIALOG_HU,    /* size */
/* it */   IDD_CBMII_SETTINGS_IO_DIALOG_IT,    /* size */
/* nl */   IDD_CBMII_SETTINGS_IO_DIALOG_NL,
/* pl */   IDD_CBMII_SETTINGS_IO_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_CBMII_SETTINGS_IO_DIALOG_SV,    /* size */
/* tr */   IDD_CBMII_SETTINGS_IO_DIALOG_TR },  /* fuzzy, size */

/* rescbm2.rc */
/* en */ { IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG,
/* da */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_NL,
/* pl */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_CBM2KBD_MAPPING_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* respet.rc */
/* en */ { IDD_PETROM_RESOURCE_DIALOG,
/* da */   IDD_PETROM_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PETROM_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_PETROM_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_PETROM_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_PETROM_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_PETROM_RESOURCE_DIALOG_NL,
/* pl */   IDD_PETROM_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PETROM_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_PETROM_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* respet.rc */
/* en */ { IDD_PETROMDRIVE_RESOURCE_DIALOG,
/* da */   IDD_PETROMDRIVE_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PETROMDRIVE_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_PETROMDRIVE_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_PETROMDRIVE_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_PETROMDRIVE_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_PETROMDRIVE_RESOURCE_DIALOG_NL,
/* pl */   IDD_PETROMDRIVE_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PETROMDRIVE_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_PETROMDRIVE_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* respet.rc */
/* en */ { IDD_PET_SETTINGS_MODEL_DIALOG,
/* da */   IDD_PET_SETTINGS_MODEL_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PET_SETTINGS_MODEL_DIALOG_DE,    /* size */
/* fr */   IDD_PET_SETTINGS_MODEL_DIALOG_FR,    /* size */
/* hu */   IDD_PET_SETTINGS_MODEL_DIALOG_HU,    /* size */
/* it */   IDD_PET_SETTINGS_MODEL_DIALOG_IT,    /* size */
/* nl */   IDD_PET_SETTINGS_MODEL_DIALOG_NL,
/* pl */   IDD_PET_SETTINGS_MODEL_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PET_SETTINGS_MODEL_DIALOG_SV,    /* size */
/* tr */   IDD_PET_SETTINGS_MODEL_DIALOG_TR },  /* fuzzy, size */

/* respet.rc */
/* en */ { IDD_PET_SETTINGS_IO_DIALOG,
/* da */   IDD_PET_SETTINGS_IO_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PET_SETTINGS_IO_DIALOG_DE,    /* size */
/* fr */   IDD_PET_SETTINGS_IO_DIALOG_FR,    /* size */
/* hu */   IDD_PET_SETTINGS_IO_DIALOG_HU,    /* size */
/* it */   IDD_PET_SETTINGS_IO_DIALOG_IT,    /* size */
/* nl */   IDD_PET_SETTINGS_IO_DIALOG_NL,
/* pl */   IDD_PET_SETTINGS_IO_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PET_SETTINGS_IO_DIALOG_SV,    /* size */
/* tr */   IDD_PET_SETTINGS_IO_DIALOG_TR },  /* fuzzy, size */

/* respet.rc */
/* en */ { IDD_PET_SETTINGS_SUPER_DIALOG,
/* da */   IDD_PET_SETTINGS_SUPER_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PET_SETTINGS_SUPER_DIALOG_DE,    /* size */
/* fr */   IDD_PET_SETTINGS_SUPER_DIALOG_FR,    /* size */
/* hu */   IDD_PET_SETTINGS_SUPER_DIALOG_HU,    /* size */
/* it */   IDD_PET_SETTINGS_SUPER_DIALOG_IT,    /* size */
/* nl */   IDD_PET_SETTINGS_SUPER_DIALOG_NL,
/* pl */   IDD_PET_SETTINGS_SUPER_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PET_SETTINGS_SUPER_DIALOG_SV,    /* size */
/* tr */   IDD_PET_SETTINGS_SUPER_DIALOG_TR },  /* fuzzy, size */

/* respet.rc */
/* en */ { IDD_PET_SETTINGS_8296_DIALOG,
/* da */   IDD_PET_SETTINGS_8296_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PET_SETTINGS_8296_DIALOG_DE,    /* size */
/* fr */   IDD_PET_SETTINGS_8296_DIALOG_FR,    /* size */
/* hu */   IDD_PET_SETTINGS_8296_DIALOG_HU,    /* size */
/* it */   IDD_PET_SETTINGS_8296_DIALOG_IT,    /* size */
/* nl */   IDD_PET_SETTINGS_8296_DIALOG_NL,
/* pl */   IDD_PET_SETTINGS_8296_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PET_SETTINGS_8296_DIALOG_SV,    /* size */
/* tr */   IDD_PET_SETTINGS_8296_DIALOG_TR },  /* fuzzy, size */

/* respet.rc */
/* en */ { IDD_PETKBD_MAPPING_SETTINGS_DIALOG,
/* da */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_NL,
/* pl */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_PETKBD_MAPPING_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resplus4.rc */
/* en */ { IDD_PLUS4ROM_RESOURCE_DIALOG,
/* da */   IDD_PLUS4ROM_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PLUS4ROM_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_PLUS4ROM_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_PLUS4ROM_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_PLUS4ROM_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_PLUS4ROM_RESOURCE_DIALOG_NL,
/* pl */   IDD_PLUS4ROM_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PLUS4ROM_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_PLUS4ROM_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* resplus4.rc */
/* en */ { IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG,
/* da */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_NL,
/* pl */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_SV,
/* tr */   IDD_PLUS4ROMDRIVE_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* resplus4.rc */
/* en */ { IDD_PLUS4_MEMORY_DIALOG,
/* da */   IDD_PLUS4_MEMORY_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PLUS4_MEMORY_DIALOG_DE,    /* size */
/* fr */   IDD_PLUS4_MEMORY_DIALOG_FR,    /* size */
/* hu */   IDD_PLUS4_MEMORY_DIALOG_HU,    /* size */
/* it */   IDD_PLUS4_MEMORY_DIALOG_IT,    /* size */
/* nl */   IDD_PLUS4_MEMORY_DIALOG_NL,
/* pl */   IDD_PLUS4_MEMORY_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PLUS4_MEMORY_DIALOG_SV,    /* size */
/* tr */   IDD_PLUS4_MEMORY_DIALOG_TR },  /* fuzzy, size */

/* resdriveplus4.rc */
/* en */ { IDD_DRIVE_SETTINGS_DIALOG_PLUS4,
/* da */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_DA,    /* fuzzy, size */
/* de */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_DE,    /* size */
/* fr */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_FR,    /* size */
/* hu */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_HU,    /* size */
/* it */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_IT,    /* size */
/* nl */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_NL,
/* pl */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_PL,    /* fuzzy, size */
/* sv */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_SV,    /* size */
/* tr */   IDD_DRIVE_SETTINGS_DIALOG_PLUS4_TR },  /* fuzzy, size */

/* resmmc64.rc */
/* en */ { IDD_MMC64_SETTINGS_DIALOG,
/* da */   IDD_MMC64_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_MMC64_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_MMC64_SETTINGS_DIALOG_FR,    /* fuzzy, size */
/* hu */   IDD_MMC64_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_MMC64_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_MMC64_SETTINGS_DIALOG_NL,    /* size */
/* pl */   IDD_MMC64_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_MMC64_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_MMC64_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resplus4.rc */
/* en */ { IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG,
/* da */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_NL,
/* pl */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_PLUS4KBD_MAPPING_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resvic20.rc */
/* en */ { IDD_VIC20ROM_RESOURCE_DIALOG,
/* da */   IDD_VIC20ROM_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_VIC20ROM_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_VIC20ROM_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_VIC20ROM_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_VIC20ROM_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_VIC20ROM_RESOURCE_DIALOG_NL,
/* pl */   IDD_VIC20ROM_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_VIC20ROM_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_VIC20ROM_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* resvic20.rc */
/* en */ { IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG,
/* da */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_DE,    /* size */
/* fr */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_FR,    /* size */
/* hu */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_NL,
/* pl */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_VIC20KBD_MAPPING_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resvic20.rc */
/* en */ { IDD_VIC20ROMDRIVE_RESOURCE_DIALOG,
/* da */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_DE,    /* size */
/* fr */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_FR,    /* size */
/* hu */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_HU,    /* size */
/* it */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_IT,    /* size */
/* nl */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_NL,
/* pl */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_SV,    /* size */
/* tr */   IDD_VIC20ROMDRIVE_RESOURCE_DIALOG_TR },  /* fuzzy, size */

/* ressidcart.rc */
/* en */ { IDD_SIDCART_SETTINGS_DIALOG,
/* da */   IDD_SIDCART_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_SIDCART_SETTINGS_DIALOG_DE,    /* fuzzy, size */
/* fr */   IDD_SIDCART_SETTINGS_DIALOG_FR,    /* fuzzy, size */
/* hu */   IDD_SIDCART_SETTINGS_DIALOG_HU,    /* size */
/* it */   IDD_SIDCART_SETTINGS_DIALOG_IT,    /* size */
/* nl */   IDD_SIDCART_SETTINGS_DIALOG_NL,    /* size */
/* pl */   IDD_SIDCART_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_SIDCART_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_SIDCART_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* ressidcart.rc */
/* en */ { IDD_SIDCARTPLUS4_SETTINGS_DIALOG,
/* da */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_DE,    /* fuzzy, size */
/* fr */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_FR,    /* fuzzy, size */
/* hu */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_HU,    /* fuzzy, size */
/* it */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_IT,    /* fuzzy, size */
/* nl */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_NL,    /* fuzzy, size */
/* pl */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_SV,    /* fuzzy, size */
/* tr */   IDD_SIDCARTPLUS4_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* resmouse.rc */
/* en */ { IDD_MOUSE_SETTINGS_DIALOG,
/* da */   IDD_MOUSE_SETTINGS_DIALOG_DA,    /* fuzzy, size */
/* de */   IDD_MOUSE_SETTINGS_DIALOG_DE,    /* fuzzy, size */
/* fr */   IDD_MOUSE_SETTINGS_DIALOG_FR,    /* fuzzy, size */
/* hu */   IDD_MOUSE_SETTINGS_DIALOG_HU,    /* fuzzy, size */
/* it */   IDD_MOUSE_SETTINGS_DIALOG_IT,    /* fuzzy, size */
/* nl */   IDD_MOUSE_SETTINGS_DIALOG_NL,
/* pl */   IDD_MOUSE_SETTINGS_DIALOG_PL,    /* fuzzy, size */
/* sv */   IDD_MOUSE_SETTINGS_DIALOG_SV,    /* size */
/* tr */   IDD_MOUSE_SETTINGS_DIALOG_TR },  /* fuzzy, size */

/* ------------------------------ MENU RESOURCES ------------------------------ */ 

/* res.rc */
/* en */ { IDR_MENUMONITOR,
/* da */   IDR_MENUMONITOR_DA,    /* fuzzy */
/* de */   IDR_MENUMONITOR_DE,
/* fr */   IDR_MENUMONITOR_FR,
/* hu */   IDR_MENUMONITOR_HU,
/* it */   IDR_MENUMONITOR_IT,
/* nl */   IDR_MENUMONITOR_NL,
/* pl */   IDR_MENUMONITOR_PL,    /* fuzzy */
/* sv */   IDR_MENUMONITOR_SV,
/* tr */   IDR_MENUMONITOR_TR },  /* fuzzy */

};

/* --------------------------------------------------------------------- */

/* codepage 28591 (ISO 8859-1) to current codepage conversion tables */

static char cp28591[256];

static const WCHAR wcp28591[256] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
    0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
    0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
    0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff
};

/* codepage 28592 (ISO 8859-2) to current codepage conversion tables */

static char cp28592[256];

static const WCHAR wcp28592[256] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
    0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f,
    0x00a0, 0x0104, 0x02d8, 0x0141, 0x00a4, 0x013d, 0x015a, 0x00a7,
    0x00a8, 0x0160, 0x015e, 0x0164, 0x0179, 0x00ad, 0x017d, 0x017b,
    0x00b0, 0x0105, 0x02db, 0x0142, 0x00b4, 0x013e, 0x015b, 0x02c7,
    0x00b8, 0x0161, 0x015f, 0x0165, 0x017a, 0x02dd, 0x017e, 0x017c,
    0x0154, 0x00c1, 0x00c2, 0x0102, 0x00c4, 0x0139, 0x0106, 0x00c7,
    0x010c, 0x00c9, 0x0118, 0x00cb, 0x011a, 0x00cd, 0x00ce, 0x010e,
    0x0110, 0x0143, 0x0147, 0x00d3, 0x00d4, 0x0150, 0x00d6, 0x00d7,
    0x0158, 0x016e, 0x00da, 0x0170, 0x00dc, 0x00dd, 0x0162, 0x00df,
    0x0155, 0x00e1, 0x00e2, 0x0103, 0x00e4, 0x013a, 0x0107, 0x00e7,
    0x010d, 0x00e9, 0x0119, 0x00eb, 0x011b, 0x00ed, 0x00ee, 0x010f,
    0x0111, 0x0144, 0x0148, 0x00f3, 0x00f4, 0x0151, 0x00f6, 0x00f7,
    0x0159, 0x016f, 0x00fa, 0x0171, 0x00fc, 0x00fd, 0x0163, 0x02d9
};

/* codepage 28599 (ISO 8859-9) to current codepage conversion tables */

static char cp28599[256];

static const WCHAR wcp28599[256] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
    0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
    0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    0x011e, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x0130, 0x015e, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
    0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x011f, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x0131, 0x015f, 0x00ff
};

char *intl_convert_cp(char *text, int cp)
{
    int len,i;
    char *cp_table;
    char *buffer;

    if (text == NULL) {
        return NULL;
    }
    len = (int)strlen(text);
    if (len == 0) {
        return NULL;
    }
    switch (cp) {
        case 28591:
            cp_table = cp28591;
            break;
        case 28592:
            cp_table = cp28592;
            break;
        case 28599:
            cp_table = cp28599;
            break;
        default:
            cp_table = cp28591;
    }
    buffer = lib_stralloc(text);
    for (i = 0; i < len; i++) {
        buffer[i] = cp_table[(unsigned char)text[i]];
    }
    return buffer;
}

/* --------------------------------------------------------------------- */

static char *intl_text_table[LAST_IDS + 16][countof(language_table)];
static BYTE text_cache[(((LAST_IDS / 16) + 1) / 8) + 1];

/* It also prepares any codepage conversion tables. */

static void intl_text_init(void)
{

    ZeroMemory(intl_text_table, sizeof(intl_text_table));
    ZeroMemory(text_cache, sizeof(text_cache));

    /* prepare the codepage 28591 (ISO 8859-1) to current codepage conversion */
    WideCharToMultiByte(CP_ACP, 0, wcp28591, 256, cp28591, 256, NULL, NULL);

    /* prepare the codepage 28592 (ISO 8859-2) to current codepage conversion */
    WideCharToMultiByte(CP_ACP, 0, wcp28592, 256, cp28592, 256, NULL, NULL);

    /* prepare the codepage 28599 (ISO 8859-9) to current codepage conversion */
    WideCharToMultiByte(CP_ACP, 0, wcp28599, 256, cp28599, 256, NULL, NULL);
}

static void intl_text_free(void)
{
    unsigned int i, j;

    for (i = 0; i < countof(language_table); i++) {
        for (j = 0; j < countof(intl_text_table); j++) {
            lib_free(intl_text_table[j][i]);
        }
    }
}

char *intl_translate_text(int en_resource)
{
    char *text;
    unsigned int i;
    int j, k;
    char temp_buffer[4098 * sizeof(TCHAR)];
    HRSRC hRes;
    HGLOBAL hGlob;
    WORD *p;
    int length;

    if (!(text_cache[en_resource >> 7] & (1 << ((en_resource >> 4) & 7)))) {
        j = (en_resource >> 4) + 1;
        for (i = 0; i < countof(language_table); i++) {
            hRes = FindResourceEx(NULL, RT_STRING, MAKEINTRESOURCE(j), (WORD)MAKELANGID(windows_to_iso[i].windows_code, SUBLANG_NEUTRAL));
            if (hRes) {
                hGlob = LoadResource(NULL, hRes);
                p = LockResource(hGlob);
                for (k = 0; k < 16; k++) {
                    ZeroMemory(temp_buffer, sizeof(temp_buffer));
                    length = *p++;
                    WideCharToMultiByte(CP_ACP, 0, p, length, temp_buffer, 4096, NULL, NULL);
                    p = p + length;
                    intl_text_table[((j - 1) << 4) + k][i] = strdup(temp_buffer);
                }
                FreeResource(hGlob);
            } else {
                for (k = 0; k < 16; k++) {
                    intl_text_table[((j - 1) << 4) + k][i] = NULL;
                }
            }
        }
        text_cache[en_resource >> 7] |= (1 << ((en_resource >> 4) & 7));
    }
    text = intl_text_table[en_resource][current_language_index];
    if (text == NULL) {
        text = intl_text_table[en_resource][0];
    }
    return text;
}

/* pre-translated main window caption text so the emulation won't
   slow down because of all the translation calls */

char *intl_speed_at_text;

/* --------------------------------------------------------------------- */

void intl_init(void)
{
    intl_text_init();
}

int intl_translate_res(int en_resource)
{
    unsigned int i;

    if (!strcmp(current_language,"en")) {
        return en_resource;
    }

    for (i = 0; i < countof(intl_table); i++) {
        if (intl_table[i][0] == en_resource) {
            return intl_table[i][current_language_index];
        }
    }
    return en_resource;
}

void intl_shutdown(void)
{
    intl_text_free();
}

/* --------------------------------------------------------------------- */

static void intl_update_pre_translated_text(void)
{
    intl_speed_at_text=intl_translate_text(IDS_S_AT_D_SPEED);
}

char *intl_arch_language_init(void)
{
/* unused current language recognition code */
#if 0
    int i;
    WORD winlang;

    winlang = GetUserDefaultLangID() & 0x3ff;
    for (i = 0; windows_to_iso[i].iso_language_code != NULL; i++) {
        if (windows_to_iso[i].windows_code == winlang) {
            return windows_to_iso[i].iso_language_code;
        }
    }
#endif
    return "en";
}

void intl_update_ui(void)
{
    intl_update_pre_translated_text();
    ui_update_menu();
}
