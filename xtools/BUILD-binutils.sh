#!/bin/bash

. BUILD-config

download http://ftp.gnu.org/gnu/binutils $binutilsdist.tar.bz2
rm -rf $builddir-binutils
mkdir -p $builddir-binutils
cd $builddir-binutils

# Untar
tar -xjf $GNUsrc/$binutilsdist.tar.bz2

# Binutils-2.18 is very old and autoconfigure was broken. Fix it
patch -p0 < $srcroot/binutils-2.18.patch

# configure, make and install binutils
mkdir -p build-binutils
cd build-binutils
../$binutilsdist/configure \
  --target=$target  \
  --prefix=$install_prefix  \
  --with-gnu-as     \
  --with-gnu-ld

make $make_option
$sudo make install
cd ..
