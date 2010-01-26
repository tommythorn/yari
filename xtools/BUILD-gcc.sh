#!/bin/bash

. BUILD-config

download http://ftp.gnu.org/gnu/gcc/$gccdist gcc-core-${gccdist##*-}.tar.bz2
rm -rf $builddir-gcc
mkdir -p $builddir-gcc
cd $builddir-gcc

tar -xjf $GNUsrc/gcc-core-${gccdist##*-}.tar.bz2

# XXX Ideally newlib and libgloss should be integrated into the gcc
# sources.

# Configure and make GCC
mkdir -p build-gcc
cd build-gcc
../$gccdist/configure --enable-languages=c \
  --prefix=$install_prefix                 \
  --with-newlib                            \
  --with-libgloss                          \
  --target=$target                         \
  --with-gnu-as                            \
  --with-gnu-ar

make $make_option
$sudo make install
cd ..
