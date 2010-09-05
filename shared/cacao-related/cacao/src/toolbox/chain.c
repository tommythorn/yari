/* src/toolbox/chain.c - management of doubly linked lists with external linking

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

   Changes: Christian Thalinger

   $Id: chain.c 4357 2006-01-22 23:33:38Z twisti $

*/


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "mm/memory.h"
#include "toolbox/chain.h"


chain *chain_new(void)
{
	chain *c;
	
	c = NEW(chain);
	c->usedump = 0;
	c->first = NULL;
	c->last = NULL;
	c->active = NULL;

	return c;
}


chain *chain_dnew(void)
{
	chain *c;
	
	c = DNEW(chain);
	c->usedump = 1;
	c->first = NULL;
	c->last = NULL;
	c->active = NULL;

	return c;
}


void chain_free(chain *c)
{
	chainlink *l;

	assert(!c->usedump);

	l = c->first;
	while (l) {
		chainlink *nextl = l->next;
		
		FREE(l, chainlink);
		l = nextl;	
	}
	
	FREE(c, chain);
}


void chain_addafter(chain *c, void *element)
{
	chainlink *active;
	chainlink *newlink;

    active = c->active;

	if (c->usedump) {
		newlink = DNEW(chainlink);

	} else {
		newlink = NEW(chainlink);
	}

	newlink->element = element;
	
	if (active) {
		newlink->next = active->next;
		newlink->prev = active;
		
		active->next = newlink;
		if (newlink->next) {
			newlink->next->prev = newlink;

		} else {
			c->last = newlink;
		}

	} else {
		newlink->next = NULL;
		newlink->prev = NULL;

		c->active = newlink;	
		c->first = newlink;
		c->last = newlink;
	}
}


void chain_addbefore(chain *c, void *element)
{
	chainlink *active;
	chainlink *newlink;

    active = c->active;

	if (c->usedump) {
		newlink = DNEW(chainlink);

	} else {
		newlink = NEW(chainlink);
	}
	
	newlink->element = element;
	
	if (active) {
		newlink->next = active;
		newlink->prev = active->prev;
		
		active->prev = newlink;
		if (newlink->prev) {
			newlink->prev->next = newlink;

		} else {
			c->first = newlink;
		}

	} else {
		newlink->next = NULL;
		newlink->prev = NULL;

		c->active = newlink;	
		c->first = newlink;
		c->last = newlink;
	}
}


void chain_addlast(chain *c, void *e)
{
	chain_last(c);
	chain_addafter(c, e);
}


void chain_addfirst(chain *c, void *e)
{
	chain_first(c);
	chain_addbefore(c, e);
}


void chain_remove(chain *c)
{
	chainlink *active;
	
	active = c->active;
	assert(active);

	if (active->next) {
		active->next->prev = active->prev;

	} else {
		c->last = active->prev;
	}
	
	if (active->prev) {
		active->prev->next = active->next;

	} else {
		c->first = active->next;
	}


	if (active->prev) {
		c->active = active->prev;

	} else {
		c->active = active->next;
	}

	if (!c->usedump)
		FREE(active, chainlink);
}


void *chain_remove_go_prev(chain *c)
{
	chain_remove(c);
	return chain_this(c);
}



void chain_removespecific(chain *c, void *e)
{
	void *ce;
	
	ce = chain_first(c);
	while (ce) {
		if (e == ce) {
			chain_remove(c);
			return;
		}

        ce = chain_next(c);
	}
}


void *chain_next(chain *c)
{
	chainlink *active;
	
	active = c->active;

	if (!active)
		return NULL;
	
	if (active->next) {
		c->active = active->next;
		return c->active->element;
	}
		
	return NULL;
}


void *chain_prev(chain *c)
{
	chainlink *active;
	
	active = c->active;

	if (!active)
		return NULL;
	
	if (active->prev) {
		c->active = active->prev;
		return c->active->element;
	}

	return NULL;
}


void *chain_this(chain *c)
{
	if (c->active)
		return c->active->element;

	return NULL;
}


void *chain_first(chain *c)
{
	c->active = c->first;

	if (c -> active)
		return c->active->element;

	return NULL;
}

void *chain_last(chain *c)
{
	c->active = c->last;

	if (c->active)
		return c->active->element;

	return NULL;
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
 */
