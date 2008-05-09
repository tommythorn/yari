/* toolbox/tree.h - binary tree management

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
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

   Contact: cacao@cacaojvm.org

   Authors: Reinhard Grafl

   $Id: tree.h 4357 2006-01-22 23:33:38Z twisti $

*/


#ifndef _TREE_H
#define _TREE_H

typedef int (*treeelementcomperator) (void *key, void * element);


typedef struct treenode {
	struct treenode *left,*right;
	struct treenode *parent;
	
	void *element;
} treenode;

typedef struct {
	int usedump;
	treeelementcomperator comperator;	

	treenode *top;
	treenode *active;
} tree;


/* function prototypes */

tree *tree_new(treeelementcomperator comperator);
tree *tree_dnew(treeelementcomperator comperator);
void tree_free(tree *t);

void tree_add(tree *t, void *element, void *key);
void *tree_find(tree *t, void *key);

void *tree_this(tree *t);
void *tree_first(tree *t);
void *tree_next(tree *t);

#endif /* _TREE_H */


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
 */
