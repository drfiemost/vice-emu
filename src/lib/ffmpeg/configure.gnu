#!/bin/sh

splitcpuos()
{
  cpu=$1
  shift
  man=$1
  shift
  if test x"$2" != "x"; then
    os=$1
    shift
    for i in $*
    do
      os="$os-$i"
    done
  else
    os=$1
  fi
}

srcdir=""
shared=no
static=no
makecommand=""
extra_generic_enables=""
extra_ffmpeg_enables=""
extra_x264_enables=""
host=""
cpu=""
os=""

for i in $*
do
  case "$i" in
    --srcdir*)
      srcdir=`echo $i | sed -e 's/^[^=]*=//g'`
      ;;
    --enable-make-command*)
      makecommand=`echo $i | sed -e 's/^[^=]*=//g'`
      ;;
    --enable-shared-ffmpeg)
      shared=yes
      ;;
    --enable-static-ffmpeg)
      static=yes
      ;;
    --enable-w32threads)
      extra_ffmpeg_enables="$extra_ffmpeg_enables $i"
      extra_x264_enables="$extra_x264_enables --enable-win32thread"
      ;;
    --host*)
      host=`echo $i | sed -e 's/^[^=]*=//g' | sed 's/-/ /g'`
      splitcpuos $host
      ;;
  esac
done

curdir=`pwd`

if [ ! -d "../liblame" ]; then
  mkdir ../liblame
fi

cd ../liblame
cur=`pwd`
if test x"$shared" = "xyes"; then
  if test x"$host" != "x"; then
    config_line="$srcdir/../liblame/configure -v --enable-shared --disable-frontend --prefix=$cur/../libffmpeg $extra_generic_enables --host=$host"
  else
    config_line="$srcdir/../liblame/configure -v --enable-shared --disable-frontend --prefix=$cur/../libffmpeg $extra_generic_enables"
  fi
else
  if test x"$host" != "x": then
    config_line="$srcdir/../liblame/configure -v --disable-shared --enable-static --disable-frontend --prefix=$cur/../libffmpeg $extra_generic_enables --host=$host"
  else
    config_line="$srcdir/../liblame/configure -v --disable-shared --enable-static --disable-frontend --prefix=$cur/../libffmpeg $extra_generic_enables"
  fi
fi
cat <<__END
Running configure in liblame with $config_line
__END

$config_line
$makecommand install

if [ ! -d "../libx264" ]; then
  mkdir ../libx264
fi

cd ../libx264
cur=`pwd`
if test x"$shared" = "xyes"; then
  if test x"$host" != "x"; then
    config_line="$srcdir/../libx264/configure --enable-shared --enable-static --prefix=$cur/../libffmpeg $extra_generic_enables $extra_x264_enables --host=$host --cross-prefix=$host-"
  else
    config_line="$srcdir/../libx264/configure --enable-shared --enable-static --prefix=$cur/../libffmpeg $extra_generic_enables $extra_x264_enables"
  fi
else
  if test x"$host" != "x"; then
    config_line="$srcdir/../libx264/configure --enable-static --prefix=$cur/../libffmpeg --enable-win32thread --host=$host --cross-prefix=$host-"
  else
    config_line="$srcdir/../libx264/configure --enable-static --prefix=$cur/../libffmpeg --enable-win32thread"
  fi
fi

cat <<__END
Running configure in libx264 with $config_line
__END

$config_line
$makecommand install

if [ ! -d "../libffmpeg" ]; then
  mkdir ../libffmpeg
fi

cd ../libffmpeg
cur=`pwd`
if test x"$shared" = "xyes"; then
  if test x"$host" != "x"; then
    config_line="$srcdir/../libffmpeg/configure --enable-libmp3lame --enable-libx264 --enable-shared --disable-static --disable-programs --enable-gpl --extra-cflags=\"-Iinclude\" --extra-ldflags=\"-Llib -Llib64\" $extra_ffmpeg_enables $extra_generic_enables --arch=$cpu --target-os=$os --cross-prefix=$host-"
  else
    config_line="$srcdir/../libffmpeg/configure --enable-libmp3lame --enable-libx264 --enable-shared --disable-static --disable-programs --enable-gpl --extra-cflags=\"-Iinclude\" --extra-ldflags=\"-Llib -Llib64\" $extra_ffmpeg_enables $extra_generic_enables"
  fi
else
  if test x"$host" != "x"; then
    config_line="$srcdir/../libffmpeg/configure --enable-libmp3lame --enable-libx264 --enable-static --disable-programs --enable-gpl --extra-cflags=\"-Iinclude\" --extra-ldflags=\"-Llib -Llib64\" $extra_ffmpeg_enables $extra_generic_enables --arch=$cpu --target-os=$os --cross-prefix=$host-"
  else
    config_line="$srcdir/../libffmpeg/configure --enable-libmp3lame --enable-libx264 --enable-static --disable-programs --enable-gpl --extra-cflags=\"-Iinclude\" --extra-ldflags=\"-Llib -Llib64\" $extra_ffmpeg_enables $extra_generic_enables"
  fi
fi

cat <<__END
Running configure in libffmpeg with $config_line
__END

$config_line
