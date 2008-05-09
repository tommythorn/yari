#!/bin/sh

${CACAO_LIBTOOLIZE} --automake
if test `uname` = 'FreeBSD'; then
    ${CACAO_ACLOCAL} -I . -I /usr/local/share/aclocal -I /usr/local/share/aclocal19
else
    ${CACAO_ACLOCAL}
fi
${CACAO_AUTOHEADER}
${CACAO_AUTOMAKE} --add-missing
${CACAO_AUTOCONF}
