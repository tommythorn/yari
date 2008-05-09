#ifndef _EMBEDDED_CLASSES_H
#define _EMBEDDED_CLASSES_H

#include "config.h"
#include "vm/types.h"

/* embedded class info structure */
struct embedded_classinfo {
	char *path;     /* path */
	char *fullname; /* full classfile name */
	char *name;     /* class name */
	s4   size;      /* binary data size */
	char *data;     /* binary data */
};

/* number of embedded classs */
extern int embedded_class_number;

/* embedded classs information */
extern struct embedded_classinfo embedded_classes[];

#endif
