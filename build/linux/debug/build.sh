#!/bin/sh

curdir=`pwd`

cd ../../..
autoreconf --force --install

cd ${curdir}

../../../configure --prefix=${curdir} \
--enable-debug \
CFLAGS="-g -Wall" \
CXXFLAGS="-g -Wall" \
CPPFLAGS= 


#make
#make install
