/* src/toolbox/avl.h - AVL tree implementation

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

   $Id: avl.h 7860 2007-05-03 12:30:05Z twisti $

*/


#ifndef _AVL_H
#define _AVL_H

#include "config.h"

#include "vm/types.h"

#include "vm/global.h"


/* define direction in an AVL node ********************************************/

#define AVL_LEFT     0
#define AVL_RIGHT    1


/* tree comparator prototype **************************************************/

typedef s4 avl_comparator(const void *treenode, const void *node);


/* forward typedefs ***********************************************************/

typedef struct avl_tree_t avl_tree_t;
typedef struct avl_node_t avl_node_t;


/* avl_tree_t *****************************************************************/

struct avl_tree_t {
#if defined(ENABLE_THREADS)
	java_objectheader *lock;            /* threads lock object                */
#endif
	avl_node_t        *root;            /* pointer to root node               */
	avl_comparator    *comparator;      /* pointer to comparison function     */
	s4                 entries;         /* contains number of entries         */
};


/* avl_node_t *****************************************************************/

struct avl_node_t {
	void       *data;                   /* pointer to data structure          */
	s4          balance;                /* the range of the field is -2...2   */
	avl_node_t *childs[2];              /* pointers to the child nodes        */
};


/* function prototypes ********************************************************/

avl_tree_t *avl_create(avl_comparator *comparator);
bool        avl_insert(avl_tree_t *tree, void *data);
void       *avl_find(avl_tree_t *tree, void *data);

#if !defined(NDEBUG)
void        avl_dump(avl_node_t* node, s4 indent);
#endif

#endif /* _AVL_H */


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
