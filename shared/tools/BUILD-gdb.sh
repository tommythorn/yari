#!/bin/bash

. BUILD-config

download http://ftp.gnu.org/gnu/gdb/$gdbdist $gdbdist.tar.bz2
rm -rf $builddir-gdb
mkdir -p $builddir-gdb
cd $builddir-gdb

tar -xjf $GNUsrc/$gdbdist.tar.bz2

mkdir -p build-gdb
cd build-gdb
# The '--disable-werror' option is necessary for Mac OS X
../$gdbdist/configure           \
  --prefix=$install_prefix      \
  --disable-werror              \
  --target=mips-elf

make $make_option
$sudo make install
cd ..
