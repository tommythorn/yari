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
   -classpath-include=*) CLASSPATH_INCLUDE="${i:19}/";;
   *) echo "Usage: [-t<dir>] [-b<dir>] [-i<dir>] [-j<number>]"
      echo "  -t<dir>                          source top-level directory. [pwd]"
      echo "  -b<dir>                          build directory. [TOPDIR/build]"
      echo "  -i<dir>                          install directory. [TOPDIR/install]"
      echo "  -j<number>                       parallel make processes. [3]"
      echo "  -m<make>                         change make command. [make]"
      exit 1;;
  esac
done;

echo "Topdir  : ${TOPDIR:="`pwd`/"}"
echo "Builddir: ${BUILDDIR="${TOPDIR}build/"}"
echo "Instdir : ${INSTDIR:="${TOPDIR}install/"}"

# source directories
BINUTILS_DIR="${TOPDIR}binutils-2.18/"
GCC_DIR="${TOPDIR}gcc-4.2.3/"
NEWLIB_DIR="${TOPDIR}newlib-1.15.0/"
CACAO_DIR="${TOPDIR}cacao/"
CLDC_DIR="${TOPDIR}phoneme_feature/cldc/src/javaapi/cldc1.1/classes"
CLASSPATH_INCLUDE="${TOPDIR}classpath-include"

# build directories
BINUTILS_BUILD="${BUILDDIR}binutils-2.18-mips-elf/"
GCC_BUILD="${BUILDDIR}gcc-4.2.3-mips-elf/"
NEWLIB_BUILD="${BUILDDIR}newlib-1.15.0-mips-elf/"
CACAO_BUILD="${BUILDDIR}cacao-yari-elf/"
CACAO_HOST_BUILD="${BUILDDIR}cacao-host/"

# misc
PATH=${INSTDIR}bin/:${PATH}

FILES="${BINUTILS_DIR} ${GCC_DIR} ${NEWLIB_DIR} ${CACAO_DIR}"
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
mkdir -p ${CACAO_BUILD}
mkdir -p ${CACAO_HOST_BUILD}

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

# configure and build cacao for the host machine (we only need cacaoh)
pushd ${CACAO_HOST_BUILD}
  if [ ! -f ${CACAO_HOST_BUILD}Makefile ]; then
    ${CACAO_DIR}/configure --with-classpath-includedir=${CLASSPATH_INCLUDE}    \
                           --enable-java=cldc1.1 --with-classpath=cldc1.1      \
                           --with-classpath-classes=${CLDC_DIR}                \
                           --disable-libjvm --disable-threads --enable-gc=none \
                           --disable-boehm-threads

    if [ ! "$?" -eq "0" ]; then
      echo "Configuring cacao for the host machine failed."
      exit 1
    fi
  fi;

  make -sj ${MINUS_J}

  if [ ! -f ${CACAO_HOST_BUILD}/src/cacaoh/cacaoh ]; then
    echo "Building cacaoh failed."
    exit 1
  fi
popd

# configure and build cacao
pushd ${CACAO_BUILD}
  if [ ! -f ${CACAO_BUILD}Makefile ]; then
    ${CACAO_DIR}/configure --with-classpath-includedir=${CLASSPATH_INCLUDE}    \
                           --enable-java=cldc1.1 --with-classpath=cldc1.1      \
                           --with-classpath-classes=${CLDC_DIR}                \
                           --disable-libjvm --disable-threads --enable-gc=none \
                           --disable-boehm-threads --enable-statistics         \
                           --disable-zlib --enable-staticvm --disable-threads  \
                           --enable-embedded-classes --enable-gc=none          \
                           --host=mips-elf                                     \
                           -with-cacaoh=${CACAO_HOST_BUILD}/src/cacaoh/cacaoh  \
                           --enable-softfloat --enable-disassembler            \
                           CFLAGS="-Os -mips1 -Tyari.ld -msoft-float"

    if [ ! "$?" -eq "0" ]; then
      echo "Configuring cacao for yari failed."
      exit 1
    fi
  fi;

  make -sj ${MINUS_J}

  if [ ! "$?" -eq "0" ]; then
    echo "Building cacao for yari failed."
    exit 1
  fi
popd

echo "DONE."
