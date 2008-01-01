#!/bin/bash

# setup some environment vars
export builddir=$PWD
export srcroot=$PWD
export target=mips-elf
export prefix=/opt/$target
export PATH=$prefix/bin:$PATH

export GNUsrc=$PWD
export gccdist=gcc-4.2.1
export binutilsdist=binutils-2.18
export newlibdist=newlib-1.15.0

cd $builddir

# Untar the files
tar -xjf $GNUsrc/gcc-core-4.2.1.tar.bz2 # XXX
tar -xjf $GNUsrc/$binutilsdist.tar.bz2
tar -xzf $GNUsrc/$newlibdist.tar.gz

# Scary stuff -- linking newlib and libgloss in the gcc source tree
ln -s $srcroot/$newlibdist/newlib $gccdist
ln -s $srcroot/$newlibdist/libgloss $gccdist

# configure, make and install binutils
mkdir -p build-binutils
cd build-binutils
../$binutilsdist/configure \
  --target=$target  \
  --prefix=$prefix  \
  --with-gnu-as     \
  --with-gnu-ld

make
sudo make install
cd ..

# Configure and make the static C lib
mkdir -p build-gcc
cd build-gcc
../$gccdist/configure --enable-languages=c \
  --with-newlib         \
  --with-libgloss       \
  --target=mips-elf     \
  --prefix=$prefix      \
  --with-gnu-as         \
  --with-gnu-ar

make
sudo make install
cd ..

# Demo
echo 'main() { printf("hw\n"); }' > hw.c
$prefix/bin/$target-gcc -Tcfe.ld hw.c -o hw

# DONE
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