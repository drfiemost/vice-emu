#!/bin/bash

rm -rf docker-build
mkdir docker-build
cd docker-build
source ../vice/build/github-actions/build-shared.sh
set_configure_options "GTK3"
ARGS=`echo $ARGS | sed s/--with-unzip-bin//g`
ARGS=`echo $ARGS | sed s/--enable-ethernet//g`
ARGS=`echo $ARGS | sed s/--with-lame//g`
ARGS=`echo $ARGS | sed s/--with-vorbis//g`
ARGS=`echo $ARGS | sed s/--with-gif//g`
export LDFLAGS="-lssp" ; ../vice/configure -v --host=x86_64-w64-mingw32 ${ARGS} 2>&1 |tee conf.log
make -j12 2>&1 | tee make.log
make bindistzip 2>&1| tee -a make.log
