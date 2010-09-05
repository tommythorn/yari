/* src/vmcore/stackmap.c - class attribute StackMapTable

   Copyright (C) 2006, 2007 R. Grafl, A. Krall, C. Kruegel, C. Oates,
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

   $Id: utf8.h 5920 2006-11-05 21:23:09Z twisti $

*/


#include "config.h"
#include "vm/types.h"

#include "mm/memory.h"

#include "vm/exceptions.h"

#include "vmcore/class.h"
#include "vmcore/method.h"
#include "vmcore/options.h"
#include "vmcore/stackmap.h"
#include "vmcore/statistics.h"
#include "vmcore/suck.h"


/* stackmap_get_verification_type_info *****************************************

   union verification_type_info {
       Top_variable_info;
	   Integer_variable_info;
	   Float_variable_info;
	   Long_variable_info;
	   Double_variable_info;
	   Null_variable_info;
	   UninitializedThis_variable_info;
	   Object_variable_info;
	   Uninitialized_variable_info;
   }

   Top_variable_info {
       u1 tag = ITEM_Top;  // 0
   }

   Integer_variable_info {
       u1 tag = ITEM_Integer;  // 1
   }

   Float_variable_info {
       u1 tag = ITEM_Float;  // 2
   }

   Long_variable_info {
       u1 tag = ITEM_Long;  // 4
   }

   Double_variable_info {
       u1 tag = ITEM_Double;  // 3
   }

   Null_variable_info {
       u1 tag = ITEM_Null;  // 5
   }

   UninitializedThis_variable_info {
       u1 tag = ITEM_UninitializedThis;  // 6
   }

   Object_variable_info {
       u1 tag = ITEM_Object;  // 7
	   u2 cpool_index;
   }

   Uninitialized_variable_info {
       u1 tag = ITEM_Uninitialized;  // 8
	   u2 offset;
   }

*******************************************************************************/

static bool stackmap_get_verification_type_info(classbuffer *cb, verification_type_info_t *verification_type_info)
{
	/* get verification type */

	if (!suck_check_classbuffer_size(cb, 1))
		return false;

	verification_type_info->tag = suck_u1(cb);

	/* process the tag */

	switch (verification_type_info->tag) {
	case ITEM_Top:
	case ITEM_Integer:
	case ITEM_Float:
	case ITEM_Long:
	case ITEM_Double:
	case ITEM_Null:
	case ITEM_UninitializedThis:
		break;

	case ITEM_Object:
		/* get constant pool index */

		if (!suck_check_classbuffer_size(cb, 2))
			return false;

		verification_type_info->Object_variable_info.cpool_index = suck_u2(cb);
		break;

	case ITEM_Uninitialized:
		/* get offset */

		if (!suck_check_classbuffer_size(cb, 2))
			return false;

		verification_type_info->Uninitialized_variable_info.offset = suck_u2(cb);
		break;
	}

	return true;
}


/* stackmap_get_same_locals_1_stack_item_frame *********************************

   same_locals_1_stack_item_frame {
       u1 frame_type = SAME_LOCALS_1_STACK_ITEM;  // 64-127
	   verification_type_info stack[1];
   }

*******************************************************************************/

static bool stackmap_get_same_locals_1_stack_item_frame(classbuffer *cb, stack_map_frame_t *stack_map_frame)
{
	same_locals_1_stack_item_frame_t *same_locals_1_stack_item_frame;

	/* for convenience */

	same_locals_1_stack_item_frame =
		&(stack_map_frame->same_locals_1_stack_item_frame);

	if (!stackmap_get_verification_type_info(cb, &(same_locals_1_stack_item_frame->stack[0])))
		return false;

	return true;
}


/* stackmap_get_same_locals_1_stack_item_frame_extended ************************

   same_locals_1_stack_item_frame_extended {
       u1 frame_type = SAME_LOCALS_1_STACK_ITEM_EXTENDED;  // 247
	   u2 offset_delta;
	   verification_type_info stack[1];
   }

*******************************************************************************/

static bool stackmap_get_same_locals_1_stack_item_frame_extended(classbuffer *cb, stack_map_frame_t *stack_map_frame)
{
	same_locals_1_stack_item_frame_extended_t *same_locals_1_stack_item_frame_extended;

	/* for convenience */

	same_locals_1_stack_item_frame_extended =
		&(stack_map_frame->same_locals_1_stack_item_frame_extended);

	/* check buffer size */

	if (!suck_check_classbuffer_size(cb, 2))
		return false;

	/* get offset delta */

	same_locals_1_stack_item_frame_extended->offset_delta = suck_u2(cb);

	/* process stack */

	if (!stackmap_get_verification_type_info(cb, &(same_locals_1_stack_item_frame_extended->stack[0])))
		return false;

	return true;
}


/* stackmap_get_chop_frame *****************************************************

   chop_frame {
       u1 frame_type = CHOP_FRAME;  // 248-250
	   u2 offset_delta;
   }

*******************************************************************************/

static bool stackmap_get_chop_frame(classbuffer *cb,
									stack_map_frame_t *stack_map_frame)
{
	chop_frame_t *chop_frame;

	/* for convenience */

	chop_frame = &(stack_map_frame->chop_frame);

	/* check buffer size */

	if (!suck_check_classbuffer_size(cb, 2))
		return false;

	/* get offset delta */

	chop_frame->offset_delta = suck_u2(cb);

	return true;
}


/* stackmap_get_same_frame_extended ********************************************

   same_frame_extended {
       u1 frame_type = SAME_FRAME_EXTENDED;  // 251
	   u2 offset_delta;
   }

*******************************************************************************/

static bool stackmap_get_same_frame_extended(classbuffer *cb,
											 stack_map_frame_t *stack_map_frame)
{
	same_frame_extended_t *same_frame_extended;

	/* for convenience */

	same_frame_extended = &(stack_map_frame->same_frame_extended);

	/* check buffer size */

	if (!suck_check_classbuffer_size(cb, 2))
		return false;

	/* get offset delta */

	same_frame_extended->offset_delta = suck_u2(cb);

	return true;
}


/* stackmap_get_append_frame ***************************************************

   append_frame {
       u1 frame_type = APPEND_FRAME;  // 252-254
	   u2 offset_delta;
	   verification_type_info locals[frame_Type - 251];
   }

*******************************************************************************/

static bool stackmap_get_append_frame(classbuffer *cb,
									  stack_map_frame_t *stack_map_frame)
{
	append_frame_t *append_frame;
	s4              number_of_locals;
	s4              i;

	/* for convenience */

	append_frame = &(stack_map_frame->append_frame);

	/* check buffer size */

	if (!suck_check_classbuffer_size(cb, 2))
		return false;

	/* get offset delta */

	append_frame->offset_delta = suck_u2(cb);

	/* allocate locals array */

	number_of_locals = append_frame->frame_type - 251;

	append_frame->locals = DMNEW(verification_type_info_t, number_of_locals);

	/* process all locals */

	for (i = 0; i < number_of_locals; i++)
		if (!stackmap_get_verification_type_info(cb, &(append_frame->locals[i])))
			return false;

	return true;
}


/* stackmap_get_full_frame *****************************************************

   full_frame {
       u1 frame_type = FULL_FRAME;
	   u2 offset_delta;
	   u2 number_of_locals;
	   verification_type_info locals[number_of_locals];
	   u2 number_of_stack_items;
	   verification_type_info stack[number_of_stack_items];
   }

*******************************************************************************/

static bool stackmap_get_full_frame(classbuffer *cb,
									stack_map_frame_t *stack_map_frame)
{
	full_frame_t *full_frame;
	s4 i;

	/* for convenience */

	full_frame = &(stack_map_frame->full_frame);

	/* check buffer size */

	if (!suck_check_classbuffer_size(cb, 2 + 2))
		return false;

	/*  get offset delta */

	stack_map_frame->full_frame.offset_delta = suck_u2(cb);

	/* get number of locals */

	full_frame->number_of_locals = suck_u2(cb);

	/* allocate locals array */

	full_frame->locals =
		DMNEW(verification_type_info_t, full_frame->number_of_locals);

	/* process all locals */

	for (i = 0; i < full_frame->number_of_locals; i++)
		if (!stackmap_get_verification_type_info(cb, &(full_frame->locals[i])))
			return false;

	/* get number of stack items */

	if (!suck_check_classbuffer_size(cb, 2))
		return false;

	full_frame->number_of_stack_items = suck_u2(cb);

	/* allocate stack array */

	full_frame->stack =
		DMNEW(verification_type_info_t, full_frame->number_of_stack_items);

	/* process all stack items */

	for (i = 0; i < full_frame->number_of_stack_items; i++)
		if (!stackmap_get_verification_type_info(cb, &(full_frame->stack[i])))
			return false;

	return true;
}


/* stackmap_load_attribute_stackmaptable ***************************************

   stack_map {
	   u2 attribute_name_index;
	   u4 attribute_length;
	   u2 number_of_entries;
	   stack_map_frame entries[number_of_entries];
   }

   union stack_map_frame {
       same_frame;
	   same_locals_1_stack_item_frame;
	   same_locals_1_stack_item_frame_extended;
	   chop_frame;
	   same_frame_extended;
	   append_frame;
	   full_frame;
   }

   same_frame {
       u1 frame_type = SAME;  // 0-63
   }

*******************************************************************************/

bool stackmap_load_attribute_stackmaptable(classbuffer *cb, methodinfo *m)
{
	classinfo       *c;
	stack_map_t     *stack_map;
	s4               i;
	u1               frame_type;

	/* get classinfo */

	c = cb->class;

	/* allocate stack map structure */

	stack_map = DNEW(stack_map_t);

	STATISTICS(size_stack_map += sizeof(stack_map_t));

	/* check buffer size */

	if (!suck_check_classbuffer_size(cb, 4 + 2))
		return false;

	/* attribute_length */

	stack_map->attribute_length = suck_u4(cb);

	if (!suck_check_classbuffer_size(cb, stack_map->attribute_length))
		return false;

	/* get number of entries */

	stack_map->number_of_entries = suck_u2(cb);

	/* process all entries */

	stack_map->entries = DMNEW(stack_map_frame_t, stack_map->number_of_entries);

	for (i = 0; i < stack_map->number_of_entries; i++) {
		/* get the frame type */

		frame_type = suck_u1(cb);

		stack_map->entries[i].frame_type = frame_type;

		/* process frame */

		if (frame_type <= FRAME_TYPE_SAME) {
			/* same_frame */
		}
		else if (frame_type <= FRAME_TYPE_SAME_LOCALS_1_STACK_ITEM) {
			/* same_locals_1_stack_item_frame */

			if (!stackmap_get_same_locals_1_stack_item_frame(cb, &(stack_map->entries[i])))
				return false;
		}
		else if (frame_type <= FRAME_TYPE_RESERVED) {
			/* reserved */

			exceptions_throw_classformaterror(c, "reserved frame type");
			return false;
		}
		else if (frame_type == FRAME_TYPE_SAME_LOCALS_1_STACK_ITEM_EXTENDED) {
			/* same_locals_1_stack_item_frame_extended */

			if (!stackmap_get_same_locals_1_stack_item_frame_extended(cb, &(stack_map->entries[i])))
				return false;
		}
		else if (frame_type <= FRAME_TYPE_CHOP) {
			/* chop_frame */

			if (!stackmap_get_chop_frame(cb, &(stack_map->entries[i])))
				return false;
		}
		else if (frame_type == FRAME_TYPE_SAME_FRAME_EXTENDED) {
			/* same_frame_extended */

			if (!stackmap_get_same_frame_extended(cb, &(stack_map->entries[i])))
				return false;
		}
		else if (frame_type <= FRAME_TYPE_APPEND) {
			/* append_frame */

			if (!stackmap_get_append_frame(cb, &(stack_map->entries[i])))
				return false;
		}
		else if (frame_type == FRAME_TYPE_FULL_FRAME) {
			/* full_frame */

			if (!stackmap_get_full_frame(cb, &(stack_map->entries[i])))
				return false;
		}
	}

	/* store stack map in method structure */

#if 0
	/* currently not used */

	m->stack_map = stack_map;
#endif

	return true;
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
