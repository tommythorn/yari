/* src/native/vm/cldc1.1/com_sun_cldc_io_j2me_socket_Protocol.c

   Copyright (C) 2007 R. Grafl, A. Krall, C. Kruegel, C. Oates,
   R. Obermaisser, M. Platter, M. Probst, S. Ring, E. Steiner,
   C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich, J. Wenninger,
   Institut f. Computersprachen - TU Wien

   This file is part of CACAO.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   $Id: java_lang_VMRuntime.c 5900 2006-11-04 17:30:44Z michi $

*/


#include "config.h"

#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "native/jni.h"

#include "vm/global.h"
#include "vm/vm.h" /* REMOVE ME: temporarily */


/*
 * Class:     com/sun/cldc/io/j2me/socket/Protocol
 * Method:    open0
 * Signature: ([BII)I
 */
JNIEXPORT s4 JNICALL Java_com_sun_cldc_io_j2me_socket_Protocol_open0(JNIEnv *env, jclass clazz, java_bytearray *hostname, s4 port, s4 mode)
{
	struct hostent *phostent;
    struct sockaddr_in serv_addr;
	char           *name;
	s4              sockfd;
	s4              result;

	/* The hostname byte-array is a NULL terminated C-string. */

	name = (char *) &(hostname->data);

	/* get the host */

	phostent = gethostbyname(name);

	if (phostent == NULL)
		return -1;

	/* fill the sockaddr structure */

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port   = htons(port);

	MCOPY(&serv_addr.sin_addr, phostent->h_addr, u1, phostent->h_length);

	/* create the socket */

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
		return -1;

	/* connect the socket */

	result = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

	if (result < 0)
		return -1;

	return sockfd;
}


/*
 * Class:     com/sun/cldc/io/j2me/socket/Protocol
 * Method:    readBuf
 * Signature: (I[BII)I
 */
JNIEXPORT s4 JNICALL Java_com_sun_cldc_io_j2me_socket_Protocol_readBuf(JNIEnv *env, jclass clazz, s4 handle, java_bytearray *b, s4 off, s4 len)
{
	void    *buf;
	ssize_t  result;

	/* get pointer to the buffer */

	buf = &(b->data[off]);

	/* receive from the socket */

	result = recv(handle, buf, len, 0);

	if (result == 0) {
		/* the peer has performed an orderly shutdown */

		return -1;
	}
	else if (result < 0) {
		vm_abort("Java_com_sun_cldc_io_j2me_socket_Protocol_readBuf: recv failed: %s", strerror(errno));
	}

	return result;
}


/*
 * Class:     com/sun/cldc/io/j2me/socket/Protocol
 * Method:    readByte
 * Signature: (I)I
 */
JNIEXPORT s4 JNICALL Java_com_sun_cldc_io_j2me_socket_Protocol_readByte(JNIEnv *env, jclass clazz, s4 handle) {
	
	char    byte;
	ssize_t result;
	
	/* receive from the socket */

	result = recv(handle, &byte, 1, 0);

	if (result == 0) {
		/* the peer has performed an orderly shutdown */

		return -1;
	}
	else if (result < 0) {
		/* should throw an IOException */

		vm_abort("Java_com_sun_cldc_io_j2me_socket_Protocol_readByte: recv failed: %s", strerror(errno));
	}

	return byte;
}


/*
 * Class:     com/sun/cldc/io/j2me/socket/Protocol
 * Method:    writeBuf
 * Signature: (I[BII)I
 */
JNIEXPORT s4 JNICALL Java_com_sun_cldc_io_j2me_socket_Protocol_writeBuf(JNIEnv *env, jclass clazz, s4 handle, java_bytearray* b, s4 off, s4 len) {

	void    *buf;
	ssize_t  result;

	/* get pointer to the buffer */

	buf = &(b->data[off]);
	
	/* send the given byte to the socket */

	result = send(handle, buf, len, 0);

	if (result < 0)
		/* should throw an IOException */

		vm_abort("Java_com_sun_cldc_io_j2me_socket_Protocol_writeBuf: send failed: %s", strerror(errno));

	return result;

}


/*
 * Class:     com/sun/cldc/io/j2me/socket/Protocol
 * Method:    writeByte
 * Signature: (II)I
 */
JNIEXPORT s4 JNICALL Java_com_sun_cldc_io_j2me_socket_Protocol_writeByte(JNIEnv *env, jclass clazz, s4 handle, s4 b)
{
	char    byte;
	ssize_t result;

	byte = (char) b;

	/* send the given byte to the socket */

	result = send(handle, &byte, 1, 0);

	if (result < 0)
		vm_abort("Java_com_sun_cldc_io_j2me_socket_Protocol_writeByte: send failed: %s", strerror(errno));

	return result;
}


/*
 * Class:     com/sun/cldc/io/j2me/socket/Protocol
 * Method:    available0
 * Signature: (I)I
 */
JNIEXPORT s4 JNICALL Java_com_sun_cldc_io_j2me_socket_Protocol_available0(JNIEnv *env, jclass clazz, s4 handle)
{
	/* NOTE: Sun doesn't have an implementation too */

	return 0;
}


/*
 * Class:     com/sun/cldc/io/j2me/socket/Protocol
 * Method:    close0
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_sun_cldc_io_j2me_socket_Protocol_close0(JNIEnv *env, jclass clazz, s4 handle)
{
	int result;

	/* close the file descriptor */

	result = close(handle);

	if (result < 0)
		vm_abort("Java_com_sun_cldc_io_j2me_socket_Protocol_close0: close failed: %s", strerror(errno));
}


/*
 * These are local overrides for various environment variables in Emacs.
 * Please do not remove this and leave it at the end of the file, where
 * Emacs will automagically detect them.
 * ---------------------------------------------------------------------
 * Local variables:
 * mode: c
 * indent-tabs-mode: t
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 * vim:noexpandtab:sw=4:ts=4:
 */
