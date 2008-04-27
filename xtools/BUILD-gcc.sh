#!/bin/bash

. BUILD-config

download http://ftp.gnu.org/gnu/gcc/$gccdist gcc-core-${gccdist##*-}.tar.bz2
rm -rf $builddir-gcc
mkdir -p $builddir-gcc
cd $builddir-gcc

tar -xjf $GNUsrc/gcc-core-${gccdist##*-}.tar.bz2

# XXX This isn't perfect. Ideally newlib and libgloss should be
# integrated into the gcc sources. As it is, we'll get (harmless?)
# build error now, but having the newlib build separate is desirable
# as well be working on this.  If you really care about these build errors,
# re-run this script after installing newlib.

# Configure and make the static C lib
mkdir -p build-gcc
cd build-gcc
../$gccdist/configure --enable-languages=c \
  --prefix=$install_prefix      \
  --with-newlib         \
  --with-libgloss       \
  --target=mips-elf     \
  --with-gnu-as         \
  --with-gnu-ar

make $make_option
$sudo make install
cd ..

# // now the tricky bit building the c++ compiler
# // if you try
# cd ../build-gcc
# ../gcc3.3/configure --enable-languages=c++
# --enable-multilib=no
# --with-cpu=mips32
# --with-newlib
# --target=mips-mips-elf
# --prefix=$prefix --with-gnu-as
# --with-gnu-ar
#
# make
