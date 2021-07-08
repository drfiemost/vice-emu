#!/bin/sh

#
# gensvnversion.sh - svnversion.h generator script
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
# Usage: gensvnversion.sh <full svn version> <output-file>
#                          $1                 $2
#

svnrevision="$1"

# a mixed range revision with local modifications may give output like this:
# 4123:4168MSP
# first split the second part from the first, then remove the non-numeric part
svnrevisionnr=`echo $svnrevision | sed 's/.*:\(.*\)/\1/' | sed 's/\([0-9]*\).*/\1/'`

# something went wrong, use 0 as the number so compile will work
if test x"$svnrevisionnr" = "x"; then
  svnrevisionnr="0"
fi

# if the output file exists, and contains the same svnrevisionnr, then exit
# early and do not produce a new output file
if test -f $2; then
    if test `grep VICE_SVN_REV_NUMBER $2 | sed 's:#define VICE_SVN_REV_NUMBER ::'` = $svnrevisionnr; then
        exit
    fi
fi

echo "generating svnversion.h"

echo "/*" > $2
echo " * svnversion.h - SVN revision defines." >> $2
echo " *" >> $2
echo " * Autogenerated by gensvnversion.sh, DO NOT EDIT !!!" >> $2
echo " *" >> $2
echo " * Written by" >> $2
echo " *  Marco van den Heuvel <blackystardust68@yahoo.com>" >> $2
echo " *" >> $2
echo " * This file is part of VICE, the Versatile Commodore Emulator." >> $2
echo " * See README for copyright notice." >> $2
echo " *" >> $2
echo " *  This program is free software; you can redistribute it and/or modify" >> $2
echo " *  it under the terms of the GNU General Public License as published by" >> $2
echo " *  the Free Software Foundation; either version 2 of the License, or" >> $2
echo " *  (at your option) any later version." >> $2
echo " *" >> $2
echo " *  This program is distributed in the hope that it will be useful," >> $2
echo " *  but WITHOUT ANY WARRANTY; without even the implied warranty of" >> $2
echo " *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" >> $2
echo " *  GNU General Public License for more details." >> $2
echo " *" >> $2
echo " *  You should have received a copy of the GNU General Public License" >> $2
echo " *  along with this program; if not, write to the Free Software" >> $2
echo " *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA" >> $2
echo " *  02111-1307  USA." >> $2
echo " *" >> $2
echo " */" >> $2
echo "" >> $2
echo "#ifndef VICE_SVNVERSION_H" >> $2
echo "#define VICE_SVNVERSION_H" >> $2
echo "" >> $2
echo "#define VICE_SVN_REV_NUMBER $svnrevisionnr" >> $2
echo "#define VICE_SVN_REV_STRING \"$svnrevision\"" >> $2
echo "" >> $2
echo "#endif" >> $2
