#!/bin/sh

# test for a libtoolize

CACAO_HAVE_LIBTOOLIZE=false

for CACAO_LIBTOOLIZE in libtoolize libtoolize15 glibtoolize; do
    if ${CACAO_LIBTOOLIZE} --version > /dev/null 2>&1; then
        CACAO_LIBTOOLIZE_VERSION=`${CACAO_LIBTOOLIZE} --version | sed 's/^.*[^0-9.]\([0-9]\{1,\}\.[0-9.]\{1,\}\).*/\1/'`
#        echo ${CACAO_LIBTOOLIZE_VERSION}
        case ${CACAO_LIBTOOLIZE_VERSION} in
            1.5* )
                CACAO_HAVE_LIBTOOLIZE=true
                break;
                ;;
        esac
    fi
done

if test ${CACAO_HAVE_LIBTOOLIZE} = false; then
    echo "No proper libtoolize was found."
    echo "You must have libtool 1.5 installed."
    exit 1
fi


# test for a aclocal

CACAO_HAVE_ACLOCAL=false

for CACAO_ACLOCAL in aclocal aclocal-1.9 aclocal19; do
    if ${CACAO_ACLOCAL} --version > /dev/null 2>&1; then
        CACAO_ACLOCAL_VERSION=`${CACAO_ACLOCAL} --version | sed 's/^[^0-9]*\([0-9.][0-9.]*\).*/\1/'`
#        echo ${CACAO_ACLOCAL_VERSION}
        case ${CACAO_ACLOCAL_VERSION} in
            1.9* | 1.1[0-9]* )
                CACAO_HAVE_ACLOCAL=true
                break;
                ;;
        esac
    fi
done

if test ${CACAO_HAVE_ACLOCAL} = false; then
    echo "No proper aclocal was found."
    echo "You must have automake 1.9 or later installed."
    exit 1
fi


# test for a autoheader

CACAO_HAVE_AUTOHEADER=false

for CACAO_AUTOHEADER in autoheader autoheader259; do
    if ${CACAO_AUTOHEADER} --version > /dev/null 2>&1; then
        CACAO_AUTOHEADER_VERSION=`${CACAO_AUTOHEADER} --version | sed 's/^[^0-9]*\([0-9.][0-9.]*\).*/\1/'`
#        echo ${CACAO_AUTOHEADER_VERSION}
        case ${CACAO_AUTOHEADER_VERSION} in
            2.59* | 2.6[0-9]* )
                CACAO_HAVE_AUTOHEADER=true
                break;
                ;;
        esac
    fi
done

if test ${CACAO_HAVE_AUTOHEADER} = false; then
    echo "No proper autoheader was found."
    echo "You must have autoconf 2.59 or later installed."
    exit 1
fi


# test for a automake

CACAO_HAVE_AUTOMAKE=false

for CACAO_AUTOMAKE in automake automake-1.9 automake19; do
    if ${CACAO_AUTOMAKE} --version > /dev/null 2>&1; then
        CACAO_AUTOMAKE_VERSION=`${CACAO_AUTOMAKE} --version | sed 's/^[^0-9]*\([0-9.][0-9.]*\).*/\1/'`
#        echo ${CACAO_AUTOMAKE_VERSION}
        case ${CACAO_AUTOMAKE_VERSION} in
            1.9* | 1.1[0-9]* )
                CACAO_HAVE_AUTOMAKE=true
                break;
                ;;
        esac
    fi
done

if test ${CACAO_HAVE_AUTOMAKE} = false; then
    echo "No proper automake was found."
    echo "You must have automake 1.9 or later installed."
    exit 1
fi


# test for a autoconf

CACAO_HAVE_AUTOCONF=false

for CACAO_AUTOCONF in autoconf autoconf259; do
    if ${CACAO_AUTOCONF} --version > /dev/null 2>&1; then
        CACAO_AUTOCONF_VERSION=`${CACAO_AUTOCONF} --version | sed 's/^[^0-9]*\([0-9.][0-9.]*\).*/\1/'`
#        echo ${CACAO_AUTOCONF_VERSION}
        case ${CACAO_AUTOCONF_VERSION} in
            2.59* | 2.6[0-9]* )
                CACAO_HAVE_AUTOCONF=true
                break;
                ;;
        esac
    fi
done

if test ${CACAO_HAVE_AUTOCONF} = false; then
    echo "No proper autoconf was found."
    echo "You must have autoconf 2.59 or later installed."
    exit 1
fi


${CACAO_LIBTOOLIZE} --automake
if test `uname` = 'FreeBSD'; then
    ${CACAO_ACLOCAL} -I m4 -I /usr/local/share/aclocal -I /usr/local/share/aclocal19
else
    ${CACAO_ACLOCAL} -I m4
fi
${CACAO_AUTOHEADER}
${CACAO_AUTOMAKE} --add-missing
${CACAO_AUTOCONF}

export CACAO_LIBTOOLIZE
export CACAO_ACLOCAL
export CACAO_AUTOHEADER
export CACAO_AUTOMAKE
export CACAO_AUTOCONF

cd src/mm/boehm-gc && ./autogen.sh && cd ../..
