#!/bin/bash

. BUILD-config

download ftp://sources.redhat.com/pub/newlib $newlibdist.tar.gz
rm -rf $builddir-newlib
mkdir -p $builddir-newlib
cd $builddir-newlib

# Untar the files
tar -xzf $GNUsrc/$newlibdist.tar.gz

# Copy over our changes
tar -C $srcroot/newlib -cvf - .|tar -C $newlibdist -xvf -

# Build newlib-1.16.0, available from RedHat:
# http://sourceware.org/newlib/
#
# As for now, yari does not support floating point operations as well as the
# special lw{l,r}, sw{l,r} instructions. Therefore some additional configure
# options are required for newlib to work correctly.

# Dynamic execution count with different newlib options
#  tictactoe: 6588425 (enable-target-optspace)
#  tictactoe: 6498234 (disable-newlib-io-float enable-target-optspace)
#  tictactoe: 4959017 ()
#  tictactoe: 4899002 (disable-newlib-io-float)
# Clearly we're paying dearly at execution time for saving space

# Configure and make the static C lib
mkdir -p build-newlib
cd build-newlib
../$newlibdist/configure    \
  --prefix=$install_prefix  \
  --target=mips-elf         \
  --enable-target-optspace
make $make_option
$sudo make install
cd ..



# Demo
cat > hw.c <<EOF
#include <stdio.h>
int main(int c, char**v)
{
    printf("%s: Hi, I'm YARI.\n", *v);
}
EOF
$target-gcc -O -Tyari.ld hw.c -o hw





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
