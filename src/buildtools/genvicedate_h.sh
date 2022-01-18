#!/bin/sh

#
# genvicedate_h.sh - 'generate vicedate.h file based on current date' script
#
# Written by
#  Marco van den Heuvel <blackystardust68@yahoo.com>
#
# This file is part of VICE, the Versatile Commodore Emulator.
# See README for copyright notice.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
#  02111-1307  USA.
#
# Usage: genvicedate_h.sh
#

# use system echo if possible, as it supports backslash expansion
if test -f /bin/echo; then
  ECHO=/bin/echo
else
  if test -f /usr/bin/echo; then
    ECHO=/usr/bin/echo
  else
    ECHO=echo
  fi
fi

year=`date +"%Y"`

exec > src/vicedate.h

$ECHO "/*"
$ECHO " * vicedate.h - various current date values and strings that can be used in code"
$ECHO " *"
$ECHO " * Autogenerated by genvicedate_h.sh, DO NOT EDIT !!!"
$ECHO " *"
$ECHO " * Written by"
$ECHO " *  Marco van den Heuvel <blackystardust68@yahoo.com>"
$ECHO " *"
$ECHO " * This file is part of VICE, the Versatile Commodore Emulator."
$ECHO " * See README for copyright notice."
$ECHO " *"
$ECHO " *  This program is free software; you can redistribute it and/or modify"
$ECHO " *  it under the terms of the GNU General Public License as published by"
$ECHO " *  the Free Software Foundation; either version 2 of the License, or"
$ECHO " *  (at your option) any later version."
$ECHO " *"
$ECHO " *  This program is distributed in the hope that it will be useful,"
$ECHO " *  but WITHOUT ANY WARRANTY; without even the implied warranty of"
$ECHO " *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"
$ECHO " *  GNU General Public License for more details."
$ECHO " *"
$ECHO " *  You should have received a copy of the GNU General Public License"
$ECHO " *  along with this program; if not, write to the Free Software"
$ECHO " *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA"
$ECHO " *  02111-1307  USA."
$ECHO " *"
$ECHO " */"
$ECHO ""
$ECHO "#ifndef VICEDATE_H"
$ECHO "#define VICEDATE_H"
$ECHO ""
$ECHO "#define VICEDATE_YEAR $year"
$ECHO "#define VICEDATE_YEAR_STR \"$year\""
$ECHO "#endif"
