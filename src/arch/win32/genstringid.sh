#!/bin/sh

#
# genmenuid.sh - Generate stringid.h
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

ids_counter=1

echo "/*"
echo " * stringid.h"
echo " *"
echo " * Autogenerated by genstringid.sh, DO NOT EDIT !!!"
echo " *"
echo " */"
echo "#ifndef VICE_STRINGID_H"
echo "#define VICE_STRINGID_H"
echo "static struct { char *str; int cmd; } idslist[] = {"

while read data
do
  ok="no"
  case ${data%%_*} in
    "IDS") echo "  { \"$data\", $ids_counter },"
           ids_counter=`expr $ids_counter + 1`
           ok="yes"
    ;;
  esac
done

echo " { NULL, 0 }"
echo "};"
echo ""
echo "#define LAST_IDS" $ids_counter
echo ""
echo "#endif"
