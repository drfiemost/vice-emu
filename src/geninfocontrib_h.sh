#!/bin/sh
#
# geninfocontrib_h.sh - infocontrib.h generator script
#
# written by Marco van den Heuvel <blackystardust68@yahoo.com>
#
# geninfocontrib_h.sh <outputtype>
#                     $1

if test "x$1" = "x"; then
  echo no output type chosen
  exit 1
fi

# extract years and name from input
extractnames()
{
   shift
   shift
   years=$1
   shift
   if test x"$years" = "x1993-1994,"; then
     years="$years $1"
     shift
   fi
   name="$*"
}

extractitem()
{
  item=`echo $* | sed -e "s/@b{//" -e "s/}//"`
}

extractlang()
{
  language=$3
}

extractyears()
{
  years=$3
}

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

checkoutput()
{
  dooutput=yes
  case "$data" in
      @c*|"@itemize @bullet"|@item|"@end itemize") dooutput=no ;;
  esac
}

splititem4()
{
  item1=$1
  item2=$2
  item3=$3
  item4=$4
}

buildlists()
{
  CORETEAM_MEMBERS=""
  EXTEAM_MEMBERS=""
  TRANSTEAM_MEMBERS=""

  for i in $MEMBERS
  do
    item=`echo $i | sed 's/+/ /g'`
    splititem4 $item
    if test x"$item1" = "xCORE"; then
      CORETEAM_MEMBERS="$CORETEAM_MEMBERS $i"
    fi
    if test x"$item1" = "xEX"; then
      EXTEAM_MEMBERS="$EXTEAM_MEMBERS $i"
    fi
    if test x"$item1" = "xTRANS"; then
      TRANSTEAM_MEMBERS="$TRANSTEAM_MEMBERS $i"
    fi
  done
}

buildallmembers()
{
  ALL_MEMBERS=""

  for i in $MEMBERS
  do
    item=`echo $i | sed 's/+/ /g'`
    splititem4 $item
    duplicate=no
    for j in $ALL_MEMBERS
    do
      if test x"$item3" = "x$j"; then
        duplicate=yes
      fi
    done
    if test x"$duplicate" = "xno"; then
      ALL_MEMBERS="$ALL_MEMBERS $item3"
    fi
  done
}

reverselist()
{
  REVLIST=""

  for i in $*
  do
    REVLIST="$i $REVLIST"
  done
}

rm -f try.tmp
$ECHO "\\\\n" >try.tmp
n1=`cat	try.tmp	| wc -c`
n2=`expr $n1 + 0`

if test x"$n2" = "x3"; then
    linefeed="\\\\n"
else
    linefeed="\\n"
fi
rm -f try.tmp

# -----------------------------------------------------------
# infocontrib.h output type

if test x"$1" = "xinfocontrib.h"; then
  $ECHO "/*"
  $ECHO " * infocontrib.h - Text of contributors to VICE, as used in info.c"
  $ECHO " *"
  $ECHO " * Autogenerated by geninfocontrib_h.sh, DO NOT EDIT !!!"
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
  $ECHO "#ifndef VICE_INFOCONTRIB_H"
  $ECHO "#define VICE_INFOCONTRIB_H"
  $ECHO ""
  $ECHO "const char info_contrib_text[] ="

  outputok=no
  coreteamsection=no
  exteamsection=no
  transteamsection=no
  docteamsection=no

  rm -f coreteam.tmp exteam.tmp transteam.tmp docteam.tmp team.tmp

  while read data
  do
    if test x"$data" = "x@c ---vice-core-team-end---"; then
      coreteamsection=no
    fi

    if test x"$data" = "x@c ---ex-team-end---"; then
      exteamsection=no
    fi

    if test x"$data" = "x@c ---translation-team-end---"; then
      transteamsection=no
    fi

    if test x"$data" = "x@c ---documentation-team-end---"; then
      docteamsection=no
    fi

    if test x"$coreteamsection" = "xyes"; then
      extractnames $data
      $ECHO >>coreteam.tmp "    { \"$years\", \"$name\", \"@b{$name}\" },"
      yearencoded=`$ECHO "$years" | sed 's/ /_/g'`
      nameencoded=`$ECHO "$name" | sed 's/ /_/g'`
      $ECHO >>team.tmp "CORE+$yearencoded+$nameencoded"
    fi

    if test x"$exteamsection" = "xyes"; then
      extractnames $data
      $ECHO >>exteam.tmp "    { \"$years\", \"$name\", \"@b{$name}\" },"
      yearencoded=`$ECHO "$years" | sed 's/ /_/g'`
      nameencoded=`$ECHO "$name" | sed 's/ /_/g'`
      $ECHO >>team.tmp "EX+$yearencoded+$nameencoded"
    fi

    if test x"$transteamsection" = "xyes"; then
      extractitem $data
      read data
      extractyears $data
      read data
      extractlang $data
      read data
      $ECHO >>transteam.tmp "    { \"$years\", \"$item\", \"$language\", \"@b{$item}\" },"
      nameencoded=`$ECHO "$item" | sed 's/ /_/g'`
      $ECHO >>team.tmp "TRANS+$years+$nameencoded+$language"
    fi

    if test x"$docteamsection" = "xyes"; then
      extractitem $data
      read data
      $ECHO >>docteam.tmp "    \"$item\","
    fi

    if test x"$data" = "x@c ---vice-core-team---"; then
      coreteamsection=yes
    fi

    if test x"$data" = "x@c ---ex-team---"; then
      exteamsection=yes
    fi

    if test x"$data" = "x@c ---translation-team---"; then
      transteamsection=yes
    fi

    if test x"$data" = "x@c ---documentation-team---"; then
      docteamsection=yes
    fi

    if test x"$data" = "x@node Copyright, Contacts, Acknowledgments, Top"; then
      $ECHO "\"$linefeed\";"
      outputok=no
    fi
    if test x"$outputok" = "xyes"; then
      checkoutput
      if test x"$dooutput" = "xyes"; then
        if test x"$data" = "x"; then
          $ECHO "\"$linefeed\""
        else
          $ECHO "\"  $data$linefeed\""
        fi
      fi
    fi
    if test x"$data" = "x@chapter Acknowledgments"; then
      outputok=yes
    fi
  done

  $ECHO ""
  $ECHO "vice_team_t core_team[] = {"
  cat coreteam.tmp
  rm -f coreteam.tmp
  $ECHO "    { NULL, NULL, NULL }"
  $ECHO "};"
  $ECHO ""
  $ECHO "vice_team_t ex_team[] = {"
  cat exteam.tmp
  rm -f exteam.tmp
  $ECHO "    { NULL, NULL, NULL }"
  $ECHO "};"
  $ECHO ""
  $ECHO "char *doc_team[] = {"
  cat docteam.tmp
  rm -f docteam.tmp
  $ECHO "    NULL"
  $ECHO "};"
  $ECHO ""
  $ECHO "vice_trans_t trans_team[] = {"
  cat transteam.tmp
  rm -f transteam.tmp
  $ECHO "    { NULL, NULL, NULL, NULL }"
  $ECHO "};"
  $ECHO "#endif"
fi

# -----------------------------------------------------------
# AUTHORS output type

if test x"$1" = "xAUTHORS"; then
  MEMBERS=`cat team.tmp`
  buildlists
  $ECHO "Core Team Members:"
  $ECHO ""
  for i in $CORETEAM_MEMBERS
  do
    decodedall=`$ECHO "$i" | sed 's/+/ /g'`
    splititem4 $decodedall
    decoded=`$ECHO "$item3" | sed 's/_/ /g'`
    $ECHO "@b{$decoded}"
  done
  $ECHO ""
  $ECHO ""
  $ECHO "Inactive/Ex Team Members:"
  $ECHO ""
  for i in $EXTEAM_MEMBERS
  do
    decodedall=`$ECHO "$i" | sed 's/+/ /g'`
    splititem4 $decodedall
    decoded=`$ECHO "$item3" | sed 's/_/ /g'`
    $ECHO "@b{$decoded}"
  done
  $ECHO ""
  $ECHO ""
  $ECHO "Translation Team Members:"
  $ECHO ""
  for i in $TRANSTEAM_MEMBERS
  do
    decodedall=`$ECHO "$i" | sed 's/+/ /g'`
    splititem4 $decodedall
    decoded=`$ECHO "$item3" | sed 's/_/ /g'`
    $ECHO "@b{$decoded}"
  done
fi

# -----------------------------------------------------------
# Mac OS X Credits.html output type

if test x"$1" = "xOSXcreditshtml"; then
  MEMBERS=`cat team.tmp`
  buildlists
  $ECHO "<html>"
  $ECHO "<head><title>VICE Credits</title></head>"
  $ECHO "<body>"
  $ECHO "<div align=\"center\">VICE Core Team Members:</div>"
  $ECHO "<ul>"
  for i in $CORETEAM_MEMBERS
  do
    decodedall=`$ECHO "$i" | sed 's/+/ /g'`
    splititem4 $decodedall
    decoded=`$ECHO "$item3" | sed 's/_/ /g'`
    $ECHO "<li>$decoded</li>"
  done
  $ECHO "</ul>"
  $ECHO "<div align=\"center\">Ex/Inactive Team Members:</div>"
  $ECHO "<ul>"
  for i in $EXTEAM_MEMBERS
  do
    decodedall=`$ECHO "$i" | sed 's/+/ /g'`
    splititem4 $decodedall
    decoded=`$ECHO "$item3" | sed 's/_/ /g'`
    $ECHO "<li>$decoded</li>"
  done
  $ECHO "</ul>"
  $ECHO "<div align=\"center\">The VICE Translation Team:</div>"
  $ECHO "<ul>"
  for i in $TRANSTEAM_MEMBERS
  do
    decodedall=`$ECHO "$i" | sed 's/+/ /g'`
    splititem4 $decodedall
    decoded=`$ECHO "$item3" | sed 's/_/ /g'`
    $ECHO "<li>$decoded</li>"
  done
  $ECHO "</ul>"
  $ECHO "</body>"
  $ECHO "</html>"
fi

# -----------------------------------------------------------
# README output type

if test x"$1" = "xREADME"; then
  MEMBERS=`cat team.tmp`
  buildlists
  outputok=no

  $ECHO ""
  $ECHO "  VICE  2.4                                                        Nov 2012"
  $ECHO ""
  $ECHO ""
  $ECHO "                 _______________"
  $ECHO "                |      ||      ||"
  $ECHO "                |      ||      ||____________________________"
  $ECHO "                |      ||      ||    ||         ||          ||"
  $ECHO "                |      ||      ||    ||    |    ||    ______||"
  $ECHO "                |      ||      ||    ||    |----||          ||"
  $ECHO "                |      ||      ||    ||    |    ||    ------||"
  $ECHO "                |              ||____||_________||__________||"
  $ECHO "                 \\            // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
  $ECHO "                  \\          //"
  $ECHO "                   \\________//   Versatile Commodore 8-bit Emulator"
  $ECHO ""
  $ECHO ""
  $ECHO ""

  old_IFS=$IFS
  IFS=''
  while read data
  do
    if test x"$data" = "x ----------------------------------------------------------------------------"; then
      outputok=yes
    fi

    if test x"$data" = "x VICE, the Versatile Commodore Emulator"; then
      IFS=$old_IFS
      $ECHO " VICE, the Versatile Commodore Emulator"
      $ECHO ""
      $ECHO "    Core Team Members:"
      for i in $CORETEAM_MEMBERS
      do
        decodedall=`$ECHO "$i" | sed 's/+/ /g'`
        splititem4 $decodedall
        decodedyear=`$ECHO "$item2" | sed 's/_/ /g'`
        decodedname=`$ECHO "$item3" | sed 's/_/ /g'`
        $ECHO "    $decodedyear $decodedname"
      done
      $ECHO ""
      $ECHO "    Inactive/Ex Team Members:"
      for i in $EXTEAM_MEMBERS
      do
        decodedall=`$ECHO "$i" | sed 's/+/ /g'`
        splititem4 $decodedall
        decodedyear=`$ECHO "$item2" | sed 's/_/ /g'`
        decodedname=`$ECHO "$item3" | sed 's/_/ /g'`
        $ECHO "    $decodedyear $decodedname"
      done
      $ECHO ""
      $ECHO "Translation Team Members:"
      $ECHO ""
      for i in $TRANSTEAM_MEMBERS
      do
        decodedall=`$ECHO "$i" | sed 's/+/ /g'`
        splititem4 $decodedall
        decodedyear=`$ECHO "$item2" | sed 's/_/ /g'`
        decodedname=`$ECHO "$item3" | sed 's/_/ /g'`
        $ECHO "    $decodedyear $decodedname"
      done
      $ECHO ""
      IFS=''
      read data
      while test x"$data" != "x  This program is free software; you can redistribute it and/or"
      do
        read data
      done
    fi

    if test x"$outputok" = "xyes"; then
      $ECHO "$data"
    fi
  done
fi

# -----------------------------------------------------------
# index.html output type

if test x"$1" = "xindexhtml"; then
  MEMBERS=`cat team.tmp`
  buildlists

  old_IFS=$IFS
  IFS=''
  while read data
  do
    if test x"$data" = "x<!--teamstart-->"; then
      IFS=$old_IFS
      $ECHO "<!--teamstart-->"
      $ECHO "<p>"
      $ECHO "Current VICE team members:"
      decodedname=""
      for i in $CORETEAM_MEMBERS
      do
        if test x"$decodedname" != "x"; then
          $ECHO "$decodedname,"
        fi
        decodedall=`$ECHO "$i" | sed 's/+/ /g'`
        splititem4 $decodedall
        decodedname=`$ECHO "$item3" | sed 's/_/ /g'`
      done
      $ECHO "$decodedname."
      $ECHO "</p>"
      $ECHO ""
      $ECHO "<p>Of course our warm thanks go to everyone who has helped us in developing"
      $ECHO "VICE during these past few years. For a more detailed list look in the"
      $ECHO "<a href=\"vice_16.html\">documentation</a>."
      $ECHO ""
      $ECHO ""
      $ECHO "<hr>"
      $ECHO ""
      $ECHO "<h1><a NAME=\"copyright\"></a>Copyright</h1>"
      $ECHO ""
      $ECHO "<p>"
      $ECHO "The VICE is copyrighted to"
      buildallmembers
      decodedname=""
      for i in $ALL_MEMBERS
      do
        if test x"$decodedname" != "x"; then
          $ECHO "$decodedname,"
        fi
        decodedname=`$ECHO "$i" | sed 's/_/ /g'`
      done
      $ECHO "$decodename."
      IFS=''
      read data
      while test x"$data" != "x<!--teamend-->"
      do
        read data
      done
    fi

    $ECHO "$data"
  done
fi

# -----------------------------------------------------------
# os2 dialogs.rc output type

if test x"$1" = "xos2dialogs"; then
  MEMBERS=`cat team.tmp`
  buildlists
  reverselist $CORETEAM_MEMBERS

  number=76
  for i in $REVLIST
  do
    number=`expr $number + 7`
  done

  old_IFS=$IFS
  IFS=''
  read data
  while test x"$data" != "x#include <pmwin.h>"
  do
    $ECHO "$data"
    read data
  done

  $ECHO "#include <pmwin.h>"
  $ECHO "#endif"
  $ECHO ""
  $ECHO "#ifdef WATCOM_COMPILE"
  $ECHO "POINTER PTR_DRAGOK \"..\\\\icons\\\\check.ico\""
  $ECHO "ICON    PTR_INFO   \"..\\\\icons\\\\info.ico\""
  $ECHO "ICON    PTR_SKULL  \"..\\\\icons\\\\skull.ico\""
  $ECHO "ICON    PTR_NOTE   \"..\\\\icons\\\\note.ico\""
  $ECHO "#else"
  $ECHO "POINTER PTR_DRAGOK \"..\\\\..\\\\icons\\\\check.ico\""
  $ECHO "ICON    PTR_INFO   \"..\\\\..\\\\icons\\\\info.ico\""
  $ECHO "ICON    PTR_SKULL  \"..\\\\..\\\\icons\\\\skull.ico\""
  $ECHO "ICON    PTR_NOTE   \"..\\\\..\\\\icons\\\\note.ico\""
  $ECHO "#endif"
  $ECHO ""
  $ECHO "#if defined __X64__ || defined __X64DTV__"
  $ECHO "#ifdef WATCOM_COMPILE"
  $ECHO "ICON    IDM_VICE2  \"..\\\\icons\\\\x64.ico\""
  $ECHO "ICON    DLG_VSID   \"..\\\\icons\\\\vsid.ico\""
  $ECHO "#else"
  $ECHO "ICON    IDM_VICE2  \"..\\\\..\\\\icons\\\\x64.ico\""
  $ECHO "ICON    DLG_VSID   \"..\\\\..\\\\icons\\\\vsid.ico\""
  $ECHO "#endif"
  $ECHO "#endif"
  $ECHO ""
  $ECHO "#ifdef __XPET__"
  $ECHO "#ifdef WATCOM_COMPILE"
  $ECHO "ICON    IDM_VICE2 \"..\\\\icons\\\\xpet.ico\""
  $ECHO "#else"
  $ECHO "ICON    IDM_VICE2 \"..\\\\..\\\\icons\\\\xpet.ico\""
  $ECHO "#endif"
  $ECHO "#endif"
  $ECHO ""
  $ECHO "#ifdef __XCBM__"
  $ECHO "#ifdef WATCOM_COMPILE"
  $ECHO "ICON    IDM_VICE2 \"..\\\\icons\\\\xcbm2.ico\""
  $ECHO "#else"
  $ECHO "ICON    IDM_VICE2 \"..\\\\..\\\\icons\\\\xcbm2.ico\""
  $ECHO "#endif"
  $ECHO "#endif"
  $ECHO ""
  $ECHO "#ifdef __X128__"
  $ECHO "#ifdef WATCOM_COMPILE"
  $ECHO "ICON    IDM_VICE2 \"..\\\\icons\\\\x128.ico\""
  $ECHO "#else"
  $ECHO "ICON    IDM_VICE2 \"..\\\\..\\\\icons\\\\x128.ico\""
  $ECHO "#endif"
  $ECHO "#endif"
  $ECHO ""
  $ECHO "#ifdef __XVIC__"
  $ECHO "#ifdef WATCOM_COMPILE"
  $ECHO "ICON    IDM_VICE2 \"..\\\\icons\\\\xvic.ico\""
  $ECHO "#else"
  $ECHO "ICON    IDM_VICE2 \"..\\\\..\\\\icons\\\\xvic.ico\""
  $ECHO "#endif"
  $ECHO "#endif"
  $ECHO ""
  $ECHO "#ifdef __XPLUS4__"
  $ECHO "#ifdef WATCOM_COMPILE"
  $ECHO "ICON    IDM_VICE2 \"..\\\\icons\\\\xplus4.ico\""
  $ECHO "#else"
  $ECHO "ICON    IDM_VICE2 \"..\\\\..\\\\icons\\\\xplus4.ico\""
  $ECHO "#endif"
  $ECHO "#endif"
  $ECHO ""

  read data
  while test x"$data" != "xDLGTEMPLATE DLG_ABOUT 850 MOVEABLE DISCARDABLE"
  do
    read data
  done

  $ECHO "DLGTEMPLATE DLG_ABOUT 850 MOVEABLE DISCARDABLE"

  while read data
  do
    if test x"$data" = "x        /* start core members */"; then
      IFS=$old_IFS
      $ECHO "        /* start core members */"
      for i in $REVLIST
      do
        decodedall=`$ECHO "$i" | sed 's/+/ /g'`
        splititem4 $decodedall
        decodedyear=`$ECHO "$item2" | sed 's/_/ /g'`
        decodedname=`$ECHO "$item3" | sed 's/_/ /g'`
        $ECHO "        TEXT(\"Copyright (c) $decodedyear $decodedname\", 15, $number, 180, 7)"
        number=`expr $number - 7`
      done
      $ECHO "        /* end core members */"
      IFS=''
      read data
      while test x"$data" != "x        /* end core members */"
      do
        read data
      done
      read data
    fi

    $ECHO "$data"
  done
fi
