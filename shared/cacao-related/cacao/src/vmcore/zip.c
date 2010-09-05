/* src/vmcore/zip.c - ZIP file handling for bootstrap classloader

   Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   J. Wenninger, Institut f. Computersprachen - TU Wien

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

   $Id: zip.c 7548 2007-03-21 13:19:44Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/mman.h>

#include "vm/types.h"

#include "toolbox/hashtable.h"

#include "mm/memory.h"

#include "vm/global.h"
#include "vm/vm.h"

#include "vmcore/suck.h"
#include "vmcore/utf8.h"
#include "vmcore/zip.h"


/* start size for classes hashtable *******************************************/

#define HASHTABLE_CLASSES_SIZE    (1 << 10)


/* info taken from:
   http://www.pkware.com/business_and_developers/developer/popups/appnote.txt
*/

/* all signatures in the ZIP file have a length of 4 bytes ********************/

#define SIGNATURE_LENGTH    4


/* Local file header ***********************************************************

   local file header signature     4 bytes  (0x04034b50)
   version needed to extract       2 bytes
   general purpose bit flag        2 bytes
   compression method              2 bytes
   last mod file time              2 bytes
   last mod file date              2 bytes
   crc-32                          4 bytes
   compressed size                 4 bytes
   uncompressed size               4 bytes
   file name length                2 bytes
   extra field length              2 bytes

   file name (variable size)
   extra field (variable size)

*******************************************************************************/

#define LFH_HEADER_SIZE              30

#define LFH_SIGNATURE                0x04034b50
#define LFH_FILE_NAME_LENGTH         26
#define LFH_EXTRA_FIELD_LENGTH       28

typedef struct lfh lfh;

struct lfh {
	u2 compressionmethod;
	u4 compressedsize;
	u4 uncompressedsize;
	u2 filenamelength;
	u2 extrafieldlength;
};


/* Central directory structure *************************************************

   [file header 1]
   .
   .
   . 
   [file header n]
   [digital signature] 
   
   File header:
   
     central file header signature   4 bytes  (0x02014b50)
     version made by                 2 bytes
     version needed to extract       2 bytes
     general purpose bit flag        2 bytes
     compression method              2 bytes
     last mod file time              2 bytes
     last mod file date              2 bytes
     crc-32                          4 bytes
     compressed size                 4 bytes
     uncompressed size               4 bytes
     file name length                2 bytes
     extra field length              2 bytes
     file comment length             2 bytes
     disk number start               2 bytes
     internal file attributes        2 bytes
     external file attributes        4 bytes
     relative offset of local header 4 bytes
   
     file name (variable size)
     extra field (variable size)
     file comment (variable size)

   Digital signature:
   
     header signature                4 bytes  (0x05054b50)
     size of data                    2 bytes
     signature data (variable size)

*******************************************************************************/

#define CDSFH_HEADER_SIZE            46

#define CDSFH_SIGNATURE              0x02014b50
#define CDSFH_COMPRESSION_METHOD     10
#define CDSFH_COMPRESSED_SIZE        20
#define CDSFH_UNCOMPRESSED_SIZE      24
#define CDSFH_FILE_NAME_LENGTH       28
#define CDSFH_EXTRA_FIELD_LENGTH     30
#define CDSFH_FILE_COMMENT_LENGTH    32
#define CDSFH_RELATIVE_OFFSET        42
#define CDSFH_FILENAME               46

typedef struct cdsfh cdsfh;

struct cdsfh {
	u2 compressionmethod;
	u4 compressedsize;
	u4 uncompressedsize;
	u2 filenamelength;
	u2 extrafieldlength;
	u2 filecommentlength;
	u4 relativeoffset;
};


/* End of central directory record *********************************************

   end of central dir signature    4 bytes  (0x06054b50)
   number of this disk             2 bytes
   number of the disk with the
   start of the central directory  2 bytes
   total number of entries in the
   central directory on this disk  2 bytes
   total number of entries in
   the central directory           2 bytes
   size of the central directory   4 bytes
   offset of start of central
   directory with respect to
   the starting disk number        4 bytes
   .ZIP file comment length        2 bytes
   .ZIP file comment       (variable size)

*******************************************************************************/

#define EOCDR_SIGNATURE              0x06054b50
#define EOCDR_ENTRIES                10
#define EOCDR_OFFSET                 16

typedef struct eocdr eocdr;

struct eocdr {
	u2 entries;
	u4 offset;
};


/* zip_open ********************************************************************

   XXX

*******************************************************************************/

hashtable *zip_open(char *path)
{
	hashtable               *ht;
	hashtable_zipfile_entry *htzfe;
	int                      fd;
	u1                       lfh_signature[SIGNATURE_LENGTH];
	off_t                    len;
	u1                      *filep;
	s4                       i;
	u1                      *p;
	eocdr                    eocdr;
	cdsfh                    cdsfh;
	const char              *filename;
	const char              *classext;
	utf                     *u;
	u4                       key;       /* hashkey computed from utf-text     */
	u4                       slot;      /* slot in hashtable                  */

	/* first of all, open the file */

	if ((fd = open(path, O_RDONLY)) == -1)
		return NULL;

	/* check for signature in first local file header */

	if (read(fd, lfh_signature, SIGNATURE_LENGTH) != SIGNATURE_LENGTH)
		return NULL;

	if (SUCK_LE_U4(lfh_signature) != LFH_SIGNATURE)
		return NULL;

	/* get the file length */

	if ((len = lseek(fd, 0, SEEK_END)) == -1)
		return NULL;

	/* we better mmap the file */

	filep = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

	/* some older compilers, like DEC OSF cc, don't like comparisons
       on void* types */

	if ((ptrint) filep == (ptrint) MAP_FAILED)
		return NULL;

	/* find end of central directory record */

	for (p = filep + len; p >= filep; p--)
		if (SUCK_LE_U4(p) == EOCDR_SIGNATURE)
			break;

	/* get number of entries in central directory */

	eocdr.entries = SUCK_LE_U2(p + EOCDR_ENTRIES);
	eocdr.offset  = SUCK_LE_U4(p + EOCDR_OFFSET);

	/* create hashtable for filenames */

	ht = NEW(hashtable);

	hashtable_create(ht, HASHTABLE_CLASSES_SIZE);

	/* add all file entries into the hashtable */

	for (i = 0, p = filep + eocdr.offset; i < eocdr.entries; i++) {
		/* check file header signature */

		if (SUCK_LE_U4(p) != CDSFH_SIGNATURE)
			return NULL;

		/* we found an entry */

		cdsfh.compressionmethod = SUCK_LE_U2(p + CDSFH_COMPRESSION_METHOD);
		cdsfh.compressedsize    = SUCK_LE_U4(p + CDSFH_COMPRESSED_SIZE);
		cdsfh.uncompressedsize  = SUCK_LE_U4(p + CDSFH_UNCOMPRESSED_SIZE);
		cdsfh.filenamelength    = SUCK_LE_U2(p + CDSFH_FILE_NAME_LENGTH);
		cdsfh.extrafieldlength  = SUCK_LE_U2(p + CDSFH_EXTRA_FIELD_LENGTH);
		cdsfh.filecommentlength = SUCK_LE_U2(p + CDSFH_FILE_COMMENT_LENGTH);
		cdsfh.relativeoffset    = SUCK_LE_U4(p + CDSFH_RELATIVE_OFFSET);

		/* create utf8 string of filename, strip .class from classes */

		filename = (const char *) (p + CDSFH_FILENAME);
		classext = filename + cdsfh.filenamelength - strlen(".class");

		/* skip directory entries */

		if (filename[cdsfh.filenamelength - 1] != '/') {
			if (strncmp(classext, ".class", strlen(".class")) == 0)
				u = utf_new(filename, cdsfh.filenamelength - strlen(".class"));
			else
				u = utf_new(filename, cdsfh.filenamelength);

			/* insert class into hashtable */

			htzfe = NEW(hashtable_zipfile_entry);

			htzfe->filename          = u;
			htzfe->compressionmethod = cdsfh.compressionmethod;
			htzfe->compressedsize    = cdsfh.compressedsize;
			htzfe->uncompressedsize  = cdsfh.uncompressedsize;
			htzfe->data              = filep + cdsfh.relativeoffset;

			/* get hashtable slot */

			key  = utf_hashkey(u->text, u->blength);
			slot = key & (ht->size - 1);

			/* insert into external chain */

			htzfe->hashlink = ht->ptr[slot];

			/* insert hashtable zipfile entry */

			ht->ptr[slot] = htzfe;
			ht->entries++;
		}

		/* move to next central directory structure file header */

		p = p +
			CDSFH_HEADER_SIZE +
			cdsfh.filenamelength +
			cdsfh.extrafieldlength +
			cdsfh.filecommentlength;
	}

	/* return pointer to hashtable */

	return ht;
}


/* zip_find ********************************************************************

   Search for the given filename in the classpath entries of a zip file.

   NOTE: The '.class' extension is stripped when reading a zip file, so if
   you want to find a .class file, you must search for its name _without_
   the '.class' extension. 
   XXX I dont like that, it makes foo and foo.class ambiguous. -Edwin

   IN:
      lce..........the classpath entries for the zip file
	  u............the filename to look for

   RETURN VALUE:
      hashtable_zipfile_entry * of the entry if found, or
	  NULL if not found

*******************************************************************************/

hashtable_zipfile_entry *zip_find(list_classpath_entry *lce, utf *u)
{
	hashtable               *ht;
	u4                       key;       /* hashkey computed from utf-text     */
	u4                       slot;      /* slot in hashtable                  */
	hashtable_zipfile_entry *htzfe;     /* hashtable element                  */

	/* get classes hashtable from the classpath entry */

	ht = lce->htclasses;

	/* get the hashtable slot of the name searched */

	key   = utf_hashkey(u->text, u->blength);
	slot  = key & (ht->size - 1);
	htzfe = ht->ptr[slot];

	/* search external hash chain for utf-symbol */

	while (htzfe) {
		if (htzfe->filename == u)
			return htzfe;

		/* next element in external chain */

		htzfe = htzfe->hashlink;
	}

	/* file not found in this archive */

	return NULL;
}


/* zip_get ********************************************************************

   XXX

*******************************************************************************/

classbuffer *zip_get(list_classpath_entry *lce, classinfo *c)
{
	hashtable_zipfile_entry *htzfe;
	lfh                      lfh;
	u1                      *indata;
	u1                      *outdata;
	z_stream                 zs;
	int                      err;
	classbuffer             *cb;

	/* try to find the class in the current archive */

	htzfe = zip_find(lce, c->name);

	if (htzfe == NULL)
		return NULL;

	/* read stuff from local file header */

	lfh.filenamelength   = SUCK_LE_U2(htzfe->data + LFH_FILE_NAME_LENGTH);
	lfh.extrafieldlength = SUCK_LE_U2(htzfe->data + LFH_EXTRA_FIELD_LENGTH);

	indata = htzfe->data +
		LFH_HEADER_SIZE +
		lfh.filenamelength +
		lfh.extrafieldlength;

	/* allocate buffer for uncompressed data */

	outdata = MNEW(u1, htzfe->uncompressedsize);

	/* how is the file stored? */

	switch (htzfe->compressionmethod) {
	case Z_DEFLATED:
		/* fill z_stream structure */

		zs.next_in   = indata;
		zs.avail_in  = htzfe->compressedsize;
		zs.next_out  = outdata;
		zs.avail_out = htzfe->uncompressedsize;

		zs.zalloc = Z_NULL;
		zs.zfree  = Z_NULL;
		zs.opaque = Z_NULL;

		/* initialize this inflate run */

		if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
			vm_abort("zip_get: inflateInit2 failed: %s", strerror(errno));

		/* decompress the file into buffer */

		err = inflate(&zs, Z_SYNC_FLUSH);

		if ((err != Z_STREAM_END) && (err != Z_OK))
			vm_abort("zip_get: inflate failed: %s", strerror(errno));

		/* finish this inflate run */

		if (inflateEnd(&zs) != Z_OK)
			vm_abort("zip_get: inflateEnd failed: %s", strerror(errno));
		break;

	case 0:
		/* uncompressed file, just copy the data */
		MCOPY(outdata, indata, u1, htzfe->compressedsize);
		break;

	default:
		vm_abort("zip_get: unknown compression method %d",
				 htzfe->compressionmethod);
	}
	
	/* allocate classbuffer */

	cb = NEW(classbuffer);

	cb->class = c;
	cb->size  = htzfe->uncompressedsize;
	cb->data  = outdata;
	cb->pos   = outdata;
	cb->path  = lce->path;

	/* return the filled classbuffer structure */

	return cb;
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
