#!/bin/bash
#
# build options
MINUS_J="3"
MAKE="make"
TARGET="mips-elf"

# parse command line options
for i in $@; do
  case $i in
   -t*) TOPDIR="${i:2}/";;
   -b*) BUILDDIR="${i:2}/";;
   -i*) INSTDIR="${i:2}/";;
   -j*) MINUS_J="${i:2}";;
   -m*) MAKE="${i:2}";;
   *) echo "Usage: [-t<dir>] [-b<dir>] [-i<dir>] [-j<number>]"
      echo "  -t<dir>          source top-level directory. [pwd]"
      echo "  -b<dir>          build directory. [TOPDIR/build]"
      echo "  -i<dir>          install directory. [TOPDIR/install]"
      echo "  -j<number>       parallel make processes. [3]"
      echo "  -m<make>         change make command. [make]"
      exit 1;;
  esac
done;

echo "Topdir  : ${TOPDIR:="`pwd`/"}"
echo "Builddir: ${BUILDDIR="${TOPDIR}build/"}"
echo "Instdir : ${INSTDIR:="${TOPDIR}install/"}"

# source directories
BINUTILS_DIR="${TOPDIR}binutils-2.17/"
GCC_DIR="${TOPDIR}gcc-4.1.1/"
NEWLIB_DIR="${TOPDIR}newlib-1.15.0/"

# build directories
BINUTILS_BUILD="${BUILDDIR}binutils-2.17-mips-elf/"
GCC_BUILD="${BUILDDIR}gcc-4.1.1-mips-elf/"
NEWLIB_BUILD="${BUILDDIR}newlib-1.15.0-mips-elf/"

# misc
PATH=${INSTDIR}bin/:${PATH}

FILES="${BINUTILS_DIR} ${GCC_DIR} ${NEWLIB_DIR}"
for i in ${FILES}; do
  if [ ! -e $i ]; then
      echo "Required file '$i' missing."
      exit 1
  fi;
done

# make directories
mkdir -p ${INSTDIR}
mkdir -p ${BINUTILS_BUILD}
mkdir -p ${GCC_BUILD}
mkdir -p ${NEWLIB_BUILD}

# configure and build binutils
pushd ${BINUTILS_BUILD}
  if [ ! -f ${BINUTILS_BUILD}Makefile ]; then
    ${BINUTILS_DIR}configure --prefix=${INSTDIR} --program-prefix=${TARGET}- --target=${TARGET}

    if [ ! "$?" -eq "0" ]; then
      echo "Configuring binutils failed."
      exit 1
    fi
  fi;

  make -sj ${MINUS_J} && make -s install

  if [ ! "$?" -eq "0" ]; then
    echo "Building binutils failed."
    exit 1
  fi
popd

# configure and build gcc
pushd ${GCC_BUILD}
  if [ ! -f ${GCC_BUILD}Makefile ]; then
    ${GCC_DIR}configure --enable-debug=full --prefix=${INSTDIR}                                      \
                        --enable-checking --disable-shared --disable-threads --without-headers       \
                        --with-newlib --enable-languages=c --with-gnu-ld --with-gnu-as               \
                        --program-prefix=${TARGET}- --target=${TARGET} 

    if [ ! "$?" -eq "0" ]; then
      echo "Configuring gcc failed."
      exit 1
    fi
  fi;

  make -sj ${MINUS_J} all-gcc CFLAGS="$CFLAGS" && make -s install-gcc

  if [ ! "$?" -eq "0" ]; then
    echo "Building gcc failed."
    exit 1
  fi
popd

# configure and build newlib
pushd ${NEWLIB_BUILD}
  if [ ! -f ${NEWLIB_BUILD}Makefile ]; then    
    ${NEWLIB_DIR}/configure --prefix=${INSTDIR} --target=${TARGET} --disable-newlib-io-float --enable-target-optspace

    if [ ! "$?" -eq "0" ]; then
      echo "Configuring newlib failed."
      exit 1
    fi
  fi;

  make -sj ${MINUS_J} && make -s install

  if [ ! "$?" -eq "0" ]; then
    echo "Building newlib failed."
    exit 1
  fi
popd

echo "DONE."

