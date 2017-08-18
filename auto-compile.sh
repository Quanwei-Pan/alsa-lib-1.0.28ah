#!/bin/bash

set -o errexit

if [ -z $args ]; then
      unset args
fi

if [ $1 == "x86" ]; then
      export CC='gcc'
      args='--disable-aload --prefix=/usr --libdir=/usr/lib --with-plugindir=/usr/lib/alsa-lib --with-pkgconfdir=/usr/lib/pkgconfig'
elif [ $1 == "arm" ]; then
      export CC='aarch64-linux-gnu-gcc'
      args='--disable-aload --prefix=/usr --libdir=/usr/lib --with-plugindir=/usr/lib/alsa-lib --with-pkgconfdir=/usr/lib/pkgconfig --host=arm-linux'
fi


touch ltconfig
libtoolize --force --copy --automake
aclocal $ACLOCAL_FLAGS
autoheader
automake --foreign --copy --add-missing
touch depcomp		# seems to be missing for old automake
autoconf
export CFLAGS='-O2 -Wall -W -pipe -g -std=gnu99'
echo "CFLAGS=$CFLAGS"
echo "./configure $args"
./configure $args || exit 1
unset CFLAGS
unset CC
if [ -z "$GITCOMPILE_NO_MAKE" ]; then
       make -j 8 V=s  #run with 8 cpu-threads
fi
