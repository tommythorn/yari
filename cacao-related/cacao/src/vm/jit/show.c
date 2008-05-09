/* src/vm/jit/show.c - showing the intermediate representation

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

   $Id$

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "threads/lock-common.h"

#include "vm/global.h"
#include "vm/builtin.h"
#include "vm/stringlocal.h"

#include "vm/jit/abi.h"
#include "vm/jit/jit.h"
#include "vm/jit/show.h"
#include "vm/jit/disass.h"
#include "vm/jit/stack.h"
#include "vm/jit/parse.h"

#include "vmcore/options.h"


/* global variables ***********************************************************/

#if defined(ENABLE_THREADS) && !defined(NDEBUG)
static java_objectheader *show_global_lock;
#endif


/* prototypes *****************************************************************/

#if !defined(NDEBUG)
static void show_variable_intern(jitdata *jd, s4 index, int stage);
#endif


/* show_init *******************************************************************

   Initialized the show subsystem (called by jit_init).

*******************************************************************************/

#if !defined(NDEBUG)
bool show_init(void)
{
#if defined(ENABLE_THREADS)
	/* initialize the show lock */

	show_global_lock = NEW(java_objectheader);

	LOCK_INIT_OBJECT_LOCK(show_global_lock);
#endif

	/* everything's ok */

	return true;
}
#endif


#if !defined(NDEBUG)
char *show_jit_type_names[] = {
	"INT",
	"LNG",
	"FLT",
	"DBL",
	"ADR",
	"??5",
	"??6",
	"??7",
	"RET"
};
char show_jit_type_letters[] = {
	'I',
	'L',
	'F',
	'D',
	'A',
	'5',
	'6',
	'7',
	'R'
};
#endif


/* show_method *****************************************************************

   Print the intermediate representation of a method.

   NOTE: Currently this function may only be called after register allocation!

*******************************************************************************/

#if !defined(NDEBUG)
void show_method(jitdata *jd, int stage)
{
	methodinfo     *m;
	codeinfo       *code;
	codegendata    *cd;
	registerdata   *rd;
	basicblock     *bptr;
	basicblock     *lastbptr;
	exception_entry *ex;
	s4              i, j;
	int             irstage;
#if defined(ENABLE_DISASSEMBLER)
	u1             *pc;
#endif

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;
	rd   = jd->rd;

	/* We need to enter a lock here, since the binutils disassembler
	   is not reentrant-able and we could not read functions printed
	   at the same time. */

	LOCK_MONITOR_ENTER(show_global_lock);

#if defined(ENABLE_INTRP)
	if (opt_intrp)
		irstage = SHOW_PARSE;
	else
#endif
		irstage = stage;

	/* get the last basic block */

	for (lastbptr = jd->basicblocks; lastbptr->next != NULL; lastbptr = lastbptr->next);

	printf("\n");

	method_println(m);

	if (jd->isleafmethod)
		printf("LEAFMETHOD\n");

	printf("\nBasic blocks: %d\n", jd->basicblockcount);
	if (stage >= SHOW_CODE) {
		printf("Code length:  %d\n", (lastbptr->mpc - jd->basicblocks[0].mpc));
		printf("Data length:  %d\n", cd->dseglen);
		printf("Stub length:  %d\n", (s4) (code->mcodelength -
										   ((ptrint) cd->dseglen + lastbptr->mpc)));
	}
	printf("Variables:       %d (%d used)\n", jd->varcount, jd->vartop);
	if (stage >= SHOW_STACK)
		printf("Max interfaces:  %d\n", jd->maxinterfaces);
	printf("Max locals:      %d\n", jd->maxlocals);
	printf("Max stack:       %d\n", m->maxstack);
	printf("Linenumbers:     %d\n", m->linenumbercount);
	printf("Branch to entry: %s\n", (jd->branchtoentry) ? "yes" : "no");
	printf("Branch to end:   %s\n", (jd->branchtoend) ? "yes" : "no");
	if (stage >= SHOW_STACK) {
		printf("Number of RETURNs: %d", jd->returncount);
		if (jd->returncount == 1)
			printf(" (block L%03d)", jd->returnblock->nr);
		printf("\n");
	}

	if (stage >= SHOW_PARSE) {
		printf("Exceptions (Number: %d):\n", jd->exceptiontablelength);
		for (ex = jd->exceptiontable; ex != NULL; ex = ex->down) {
			printf("    L%03d ... ", ex->start->nr );
			printf("L%03d  = ", ex->end->nr);
			printf("L%03d", ex->handler->nr);
			printf("  (catchtype: ");
			if (ex->catchtype.any)
				if (IS_CLASSREF(ex->catchtype))
					class_classref_print(ex->catchtype.ref);
				else
					class_print(ex->catchtype.cls);
			else
				printf("ANY");
			printf(")\n");
		}
	}
	
	if (irstage >= SHOW_PARSE && rd && jd->localcount > 0) {
		printf("Local Table:\n");
		for (i = 0; i < jd->localcount; i++) {
			printf("   %3d: ", i);

#if defined(ENABLE_JIT) && defined(ENABLE_DISASSEMBLER)
# if defined(ENABLE_INTRP)
			if (!opt_intrp) {
# endif
				printf("   (%s) ", show_jit_type_names[VAR(i)->type]);
				if (irstage >= SHOW_REGS)
					show_allocation(VAR(i)->type, VAR(i)->flags, VAR(i)->vv.regoff);
				printf("\n");
# if defined(ENABLE_INTRP)
			}
# endif
#endif /* defined(ENABLE_JIT) && defined(ENABLE_DISASSEMBLER) */
		}
		printf("\n");
	}

	if (jd->maxlocals > 0 && jd->local_map != NULL) {
		printf("Local Map:\n");
		printf("    index ");
		for (j = 0; j < jd->maxlocals; j++) {
			printf(" [%2d]", j);
		}
		printf("\n");
		for (i = 0; i < 5; i++) {
			printf("    %5s ",show_jit_type_names[i]);
			for (j = 0; j < jd->maxlocals; j++) {
				if (jd->local_map[j*5+i] == UNUSED)
					printf("  -- ");
				else
					printf("%4i ",jd->local_map[j*5+i]);
			}
			printf("\n");
		}
		printf("\n");
	}

	if (jd->maxinterfaces > 0 && jd->interface_map && irstage >= SHOW_STACK) {
		bool exist = false;
		interface_info *mapptr = jd->interface_map;
		
		/* look if there exist any INOUTS */
		for (i = 0; (i < (5 * jd->maxinterfaces)) && !exist; i++, mapptr++)
			exist = (mapptr->flags != UNUSED);

		if (exist) {
			printf("Interface Table: (In/Outvars)\n");
			printf("    depth ");
			for (j = 0; j < jd->maxinterfaces; j++) {
				printf("      [%2d]", j);
			}
			printf("\n");

			for (i = 0; i < 5; i++) {
				printf("    %5s      ",show_jit_type_names[i]);
				for (j = 0; j < jd->maxinterfaces; j++) {
					s4 flags  = jd->interface_map[j*5+i].flags;
					s4 regoff = jd->interface_map[j*5+i].regoff;
					if (flags == UNUSED)
						printf("  --      ");
					else {
						int ch;

						if (irstage >= SHOW_REGS) {
							if (flags & SAVEDVAR) {
								if (flags & INMEMORY)
									ch = 'M';
								else
									ch = 'R';
							}
							else {
								if (flags & INMEMORY)
									ch = 'm';
								else
									ch = 'r';
							}
							printf("%c%03d(", ch, regoff);
							show_allocation(i, flags, regoff);
							printf(") ");
						}
						else {
							if (flags & SAVEDVAR)
								printf("  I       ");
							else
								printf("  i       ");
						}
					}
				}
				printf("\n");
			}
			printf("\n");
		}
	}

	if (rd->memuse && irstage >= SHOW_REGS) {
		int max;

		max = rd->memuse;
		printf("Stack slots: (memuse=%d", rd->memuse);
		if (irstage >= SHOW_CODE) {
			printf(", stackframesize=%d", cd->stackframesize);
			max = cd->stackframesize;
		}
		printf(")\n");
		for (i = 0; i < max; ++i) {
#if defined(HAS_4BYTE_STACKSLOT)
			printf("    M%02d = 0x%02x(sp): ", i, i * 4);
#else
			printf("    M%02d = 0x%02x(sp): ", i, i * 8);
#endif
			for (j = 0; j < jd->vartop; ++j) {
				varinfo *v = VAR(j);
				if ((v->flags & INMEMORY) && (v->vv.regoff == i)) {
					show_variable(jd, j, irstage);
					putchar(' ');
				}
			}

			printf("\n");

		}
		printf("\n");
	}

#if defined(ENABLE_REPLACEMENT)
	if (code->rplpoints) {
		printf("Replacement Points:\n");
		replace_show_replacement_points(code);
		printf("\n");
	}
#endif /* defined(ENABLE_REPLACEMENT) */

#if defined(ENABLE_DISASSEMBLER)
	/* show code before first basic block */

	if ((stage >= SHOW_CODE) && JITDATA_HAS_FLAG_SHOWDISASSEMBLE(jd)) {
		pc = (u1 *) ((ptrint) code->mcode + cd->dseglen);

		for (; pc < (u1 *) ((ptrint) code->mcode + cd->dseglen + jd->basicblocks[0].mpc);)
			DISASSINSTR(pc);

		printf("\n");
	}
#endif

	/* show code of all basic blocks */

	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next)
		show_basicblock(jd, bptr, stage);

#if defined(ENABLE_DISASSEMBLER)
	/* show stubs code */

	if (stage >= SHOW_CODE && opt_showdisassemble && opt_showexceptionstubs) {
		printf("\nStubs code:\n");
		printf("Length: %d\n\n", (s4) (code->mcodelength -
									   ((ptrint) cd->dseglen + lastbptr->mpc)));

		pc = (u1 *) ((ptrint) code->mcode + cd->dseglen + lastbptr->mpc);

		for (; (ptrint) pc < ((ptrint) code->mcode + code->mcodelength);)
			DISASSINSTR(pc);

		printf("\n");
	}
#endif

	LOCK_MONITOR_EXIT(show_global_lock);

	/* finally flush the output */

	fflush(stdout);
}
#endif /* !defined(NDEBUG) */


#if !defined(NDEBUG) && defined(ENABLE_INLINING)
static void show_inline_info(jitdata *jd, insinfo_inline *ii, s4 opcode, s4 stage)
{
	s4 *jl;
	s4 n;

	printf("(pt %d+%d+%d st ", 
			ii->throughcount - (ii->stackvarscount - ii->paramcount),
			ii->stackvarscount - ii->paramcount,
			ii->paramcount);
	show_variable_array(jd, ii->stackvars, ii->stackvarscount, stage);

	if (opcode == ICMD_INLINE_START || opcode == ICMD_INLINE_END) {
		printf(" jl ");
		jl = (opcode == ICMD_INLINE_START) ? ii->javalocals_start : ii->javalocals_end;
		n = (opcode == ICMD_INLINE_START) ? ii->method->maxlocals : ii->outer->maxlocals;
		show_javalocals_array(jd, jl, n, stage);
	}

	printf(") ");

#if 0
	printf("(");
	method_print(ii->outer);
	printf(" ==> ");
#endif

	method_print(ii->method);
}
#endif /* !defined(NDEBUG) && defined(ENABLE_INLINING) */


/* show_basicblock *************************************************************

   Print the intermediate representation of a basic block.

   NOTE: Currently this function may only be called after register allocation!

*******************************************************************************/

#if !defined(NDEBUG)
void show_basicblock(jitdata *jd, basicblock *bptr, int stage)
{
	codeinfo    *code;
	codegendata *cd;
	s4           i;
	bool         deadcode;
	instruction *iptr;
	int          irstage;
#if defined(ENABLE_DISASSEMBLER)
	methodinfo  *m;                     /* this is only a dummy               */
	u1          *pc;
	s4           linenumber;
	s4           currentlinenumber;
#endif

	/* get required compiler data */

	code = jd->code;
	cd   = jd->cd;

	if (bptr->flags != BBDELETED) {
#if defined(ENABLE_INTRP)
		if (opt_intrp) {
			deadcode = false;
			irstage = SHOW_PARSE;
		}
		else
#endif
		{
			deadcode = (bptr->flags < BBREACHED);
			irstage = stage;
		}

		printf("======== %sL%03d ======== %s(flags: %d, bitflags: %01x, next: %d, type: ",
#if defined(ENABLE_REPLACEMENT)
				(bptr->bitflags & BBFLAG_REPLACEMENT) ? "<REPLACE> " : 
#endif
														"",
			   bptr->nr, 
			   (deadcode && stage >= SHOW_STACK) ? "DEADCODE! " : "",
			   bptr->flags, bptr->bitflags, 
			   (bptr->next) ? (bptr->next->nr) : -1);

		switch (bptr->type) {
		case BBTYPE_STD:
			printf("STD");
			break;
		case BBTYPE_EXH:
			printf("EXH");
			break;
		case BBTYPE_SBR:
			printf("SBR");
			break;
		}

		printf(", icount: %d", bptr->icount);

		if (irstage >= SHOW_CFG) {
			printf(", preds: %d [ ", bptr->predecessorcount);

			for (i = 0; i < bptr->predecessorcount; i++)
				printf("%d ", bptr->predecessors[i]->nr);

			printf("]");
		}

		printf("):");

		if (bptr->original)
			printf(" (clone of L%03d)", bptr->original->nr);
		else {
			basicblock *b = bptr->copied_to;
			if (b) {
				printf(" (copied to ");
				for (; b; b = b->copied_to)
					printf("L%03d ", b->nr);
				printf(")");
			}
		}

		printf("\n");

		if (irstage >= SHOW_STACK) {
			printf("IN:  ");
			show_variable_array(jd, bptr->invars, bptr->indepth, irstage);
			printf(" javalocals: ");
			show_javalocals_array(jd, bptr->javalocals, bptr->method->maxlocals, irstage);
			printf("\n");
		}

#if defined(ENABLE_INLINING)
		if (bptr->inlineinfo) {
			printf("inlineinfo: ");
			show_inline_info(jd, bptr->inlineinfo, -1, irstage);
			printf("\n");
		}
#endif /* defined(ENABLE_INLINING) */

		iptr = bptr->iinstr;

		for (i = 0; i < bptr->icount; i++, iptr++) {
			printf("%4d:%4d:  ", iptr->line, iptr->flags.bits >> INS_FLAG_ID_SHIFT);

			show_icmd(jd, iptr, deadcode, irstage);
			printf("\n");
		}

		if (irstage >= SHOW_STACK) {
			printf("OUT: ");
			show_variable_array(jd, bptr->outvars, bptr->outdepth, irstage);
			printf("\n");
		}

#if defined(ENABLE_DISASSEMBLER)
		if ((stage >= SHOW_CODE) && JITDATA_HAS_FLAG_SHOWDISASSEMBLE(jd) &&
			(!deadcode)) 
		{
			printf("\n");
			pc         = (u1 *) (code->mcode + cd->dseglen + bptr->mpc);
			linenumber = 0;

			if (bptr->next != NULL) {
				for (; pc < (u1 *) (code->mcode + cd->dseglen + bptr->next->mpc);) {
					currentlinenumber =
						dseg_get_linenumber_from_pc(&m, code->entrypoint, pc);

					if (currentlinenumber != linenumber) {
						linenumber = currentlinenumber;
						printf("%4d:\n", linenumber);
					}

					DISASSINSTR(pc);
				}
			}
			else {
				for (; pc < (u1 *) (code->mcode + code->mcodelength);) {
					currentlinenumber =
						dseg_get_linenumber_from_pc(&m, code->entrypoint, pc);

					if (currentlinenumber != linenumber) {
						linenumber = currentlinenumber;
						printf("%4d:\n", linenumber);
					}

					DISASSINSTR(pc);
				}
			}
			printf("\n");
		}
#endif
	}
}
#endif /* !defined(NDEBUG) */


/* show_icmd *******************************************************************

   Print the intermediate representation of an instruction.

   NOTE: Currently this function may only be called after register allocation!

*******************************************************************************/

#if !defined(NDEBUG)

#define SHOW_TARGET(target)                                          \
        if (stage >= SHOW_PARSE) {                                   \
            printf("--> L%03d ", (target).block->nr);                \
        }                                                            \
        else {                                                       \
            printf("--> insindex %d ", (target).insindex);           \
        }

#define SHOW_INT_CONST(val)                                          \
        if (stage >= SHOW_PARSE) {                                   \
            printf("%d (0x%08x) ", (val), (val));                    \
        }                                                            \
        else {                                                       \
            printf("iconst ");                                       \
        }

#if SIZEOF_VOID_P == 4
#define SHOW_LNG_CONST(val)                                          \
        if (stage >= SHOW_PARSE)                                     \
            printf("%lld (0x%016llx) ", (val), (val));               \
        else                                                         \
            printf("lconst ");
#else
#define SHOW_LNG_CONST(val)                                          \
        if (stage >= SHOW_PARSE)                                     \
            printf("%ld (0x%016lx) ", (val), (val));                 \
        else                                                         \
            printf("lconst ");
#endif

#if SIZEOF_VOID_P == 4
#define SHOW_ADR_CONST(val)                                          \
        if (stage >= SHOW_PARSE)                                     \
            printf("0x%08x ", (ptrint) (val));                       \
        else                                                         \
            printf("aconst ");
#else
#define SHOW_ADR_CONST(val)                                          \
        if (stage >= SHOW_PARSE)                                     \
            printf("0x%016lx ", (ptrint) (val));                     \
        else                                                         \
            printf("aconst ");
#endif

#define SHOW_FLT_CONST(val)                                          \
        if (stage >= SHOW_PARSE) {                                   \
            imm_union v;                                             \
            v.f = (val);                                             \
            printf("%g (0x%08x) ", (val), v.i);                      \
        }                                                            \
        else {                                                       \
            printf("fconst ");                                       \
        }

#if SIZEOF_VOID_P == 4
#define SHOW_DBL_CONST(val)                                          \
        if (stage >= SHOW_PARSE) {                                   \
            imm_union v;                                             \
            v.d = (val);                                             \
            printf("%g (0x%016llx) ", (val), v.l);                   \
        }                                                            \
        else                                                         \
            printf("dconst ");
#else
#define SHOW_DBL_CONST(val)                                          \
        if (stage >= SHOW_PARSE) {                                   \
            imm_union v;                                             \
            v.d = (val);                                             \
            printf("%g (0x%016lx) ", (val), v.l);                    \
        }                                                            \
        else                                                         \
            printf("dconst ");
#endif

#define SHOW_INDEX(index)                                            \
        if (stage >= SHOW_PARSE) {                                   \
            printf("%d ", index);                                    \
        }                                                            \
        else {                                                       \
            printf("index");                                         \
        }

#define SHOW_STRING(val)                                             \
        if (stage >= SHOW_PARSE) {                                   \
            putchar('"');                                            \
            utf_display_printable_ascii(                             \
               javastring_toutf((java_objectheader *)(val), false)); \
            printf("\" ");                                           \
        }                                                            \
        else {                                                       \
            printf("string ");                                       \
        }

#define SHOW_CLASSREF_OR_CLASSINFO(c)                                \
        if (stage >= SHOW_PARSE) {                                   \
            if (IS_CLASSREF(c))                                      \
                class_classref_print(c.ref);                         \
            else                                                     \
                class_print(c.cls);                                  \
            putchar(' ');                                            \
        }                                                            \
        else {                                                       \
            printf("class ");                                        \
        }

#define SHOW_FIELD(fmiref)                                           \
        if (stage >= SHOW_PARSE) {                                   \
            field_fieldref_print(fmiref);                            \
            putchar(' ');                                            \
        }                                                            \
        else {                                                       \
            printf("field ");                                        \
        }

#define SHOW_VARIABLE(v)                                             \
    show_variable(jd, (v), stage)

#define SHOW_S1(iptr)                                                \
        if (stage >= SHOW_STACK) {                                   \
            SHOW_VARIABLE(iptr->s1.varindex);                        \
        }

#define SHOW_S2(iptr)                                                \
        if (stage >= SHOW_STACK) {                                   \
            SHOW_VARIABLE(iptr->sx.s23.s2.varindex);                 \
        }

#define SHOW_S3(iptr)                                                \
    if (stage >= SHOW_STACK) {                                       \
        SHOW_VARIABLE(iptr->sx.s23.s3.varindex);                     \
    }

#define SHOW_DST(iptr)                                               \
    if (stage >= SHOW_STACK) {                                       \
        printf("=> ");                                               \
        SHOW_VARIABLE(iptr->dst.varindex);                           \
    }

#define SHOW_S1_LOCAL(iptr)                                          \
    if (stage >= SHOW_STACK) {                                       \
        printf("L%d ", iptr->s1.varindex);                           \
    }                                                                \
    else {                                                           \
        printf("JavaL%d ", iptr->s1.varindex);                       \
    }

#define SHOW_DST_LOCAL(iptr)                                         \
    if (stage >= SHOW_STACK) {                                       \
        printf("=> L%d ", iptr->dst.varindex);                       \
    }                                                                \
    else {                                                           \
        printf("=> JavaL%d ", iptr->dst.varindex);                   \
    }

void show_allocation(s4 type, s4 flags, s4 regoff)
{
	if (type == TYPE_RET) {
		printf("N/A");
		return;
	}

	if (flags & INMEMORY) {
		printf("M%02d", regoff);
		return;
	}

#ifdef HAS_ADDRESS_REGISTER_FILE
	if (type == TYPE_ADR) {
		printf("R%02d", regoff);
		return;
	}
#endif

	if (IS_FLT_DBL_TYPE(type)) {
		printf("F%02d", regoff);
		return;
	}

#if defined(SUPPORT_COMBINE_INTEGER_REGISTERS)
	if (IS_2_WORD_TYPE(type)) {
# if defined(ENABLE_JIT) && defined(ENABLE_DISASSEMBLER)
#  if defined(ENABLE_INTRP)
		if (opt_intrp)
			printf("%3d/%3d", GET_LOW_REG(regoff),
					GET_HIGH_REG(regoff));
		else
#  endif
			printf("%3s/%3s", abi_registers_integer_name[GET_LOW_REG(regoff)],
				   abi_registers_integer_name[GET_HIGH_REG(regoff)]);
# else
		printf("%3d/%3d", GET_LOW_REG(regoff),
			   GET_HIGH_REG(regoff));
# endif
		return;
	} 
#endif /* defined(SUPPORT_COMBINE_INTEGER_REGISTERS) */

#if defined(ENABLE_JIT) && defined(ENABLE_DISASSEMBLER)
# if defined(ENABLE_INTRP)
	if (opt_intrp)
		printf("%3d", regoff);
	else
# endif
		printf("%3s", abi_registers_integer_name[regoff]);
#else
	printf("%3d", regoff);
#endif
}

void show_variable(jitdata *jd, s4 index, int stage)
{
	show_variable_intern(jd, index, stage);
	putchar(' ');
}

static void show_variable_intern(jitdata *jd, s4 index, int stage)
{
	char type;
	char kind;
	varinfo *v;

	if (index < 0 || index >= jd->vartop) {
		printf("<INVALID INDEX:%d>", index);
		return;
	}

	v = VAR(index);

	switch (v->type) {
		case TYPE_INT: type = 'i'; break;
		case TYPE_LNG: type = 'l'; break;
		case TYPE_FLT: type = 'f'; break;
		case TYPE_DBL: type = 'd'; break;
		case TYPE_ADR: type = 'a'; break;
		case TYPE_RET: type = 'r'; break;
		default:       type = '?';
	}

	if (index < jd->localcount) {
		kind = 'L';
		if (v->flags & (PREALLOC | INOUT))
				printf("<INVALID FLAGS!>");
	}
	else {
		if (v->flags & PREALLOC) {
			kind = 'A';
			if (v->flags & INOUT) {
				/* PREALLOC is used to avoid allocation of TYPE_RET */
				if (v->type == TYPE_RET)
					kind = 'i';
				else
					printf("<INVALID FLAGS!>");
			}
		}
		else if (v->flags & INOUT)
			kind = 'I';
		else
			kind = 'T';
	}

	printf("%c%c%d", kind, type, index);

	if (v->flags & SAVEDVAR)
		putchar('!');

	if (stage >= SHOW_REGS || (v->flags & PREALLOC)) {
		putchar('(');
		show_allocation(v->type, v->flags, v->vv.regoff);
		putchar(')');
	}

	if (v->type == TYPE_RET && (v->flags & PREALLOC)) {
		printf("(L%03d)", v->vv.retaddr->nr);
	}
}

static void show_variable_array_intern(jitdata *jd, s4 *vars, int n, int stage,
									   bool javalocals)
{
	int i;
	int nr;

	if (vars == NULL) {
		printf("<null>");
		return;
	}

	printf("[");
	for (i=0; i<n; ++i) {
		if (i)
			putchar(' ');
		if (vars[i] < 0) {
			if (vars[i] == UNUSED)
				putchar('-');
			else if (javalocals) {
				nr = RETADDR_FROM_JAVALOCAL(vars[i]);
				printf("ret(L%03d)", nr);
			}
			else {
				printf("<INVALID INDEX:%d>", vars[i]);
			}
		}
		else
			show_variable_intern(jd, vars[i], stage);
	}
	printf("]");
}

void show_variable_array(jitdata *jd, s4 *vars, int n, int stage)
{
	show_variable_array_intern(jd, vars, n, stage, false);
}

void show_javalocals_array(jitdata *jd, s4 *vars, int n, int stage)
{
	show_variable_array_intern(jd, vars, n, stage, true);
}

void show_icmd(jitdata *jd, instruction *iptr, bool deadcode, int stage)
{
	u2                 opcode;
	branch_target_t   *table;
	lookup_target_t   *lookup;
	constant_FMIref   *fmiref;
	s4                *argp;
	s4                 i;

	/* get the opcode and the condition */

	opcode    =  iptr->opc;

	printf("%s ", icmd_table[opcode].name);

	if (stage < SHOW_PARSE)
		return;

	if (deadcode)
		stage = SHOW_PARSE;

	/* Print the condition for conditional instructions. */

	/* XXX print condition from flags */

	if (iptr->flags.bits & INS_FLAG_UNRESOLVED)
		printf("(UNRESOLVED) ");

	switch (opcode) {

	case ICMD_POP:
	case ICMD_CHECKNULL:
		SHOW_S1(iptr);
		break;

		/* unary */
	case ICMD_ARRAYLENGTH:
	case ICMD_INEG:
	case ICMD_LNEG:
	case ICMD_FNEG:
	case ICMD_DNEG:
	case ICMD_I2L:
	case ICMD_I2F:
	case ICMD_I2D:
	case ICMD_L2I:
	case ICMD_L2F:
	case ICMD_L2D:
	case ICMD_F2I:
	case ICMD_F2L:
	case ICMD_F2D:
	case ICMD_D2I:
	case ICMD_D2L:
	case ICMD_D2F:
	case ICMD_INT2BYTE:
	case ICMD_INT2CHAR:
	case ICMD_INT2SHORT:
		SHOW_S1(iptr);
		SHOW_DST(iptr);
		break;

		/* binary */
	case ICMD_IADD:
	case ICMD_LADD:
	case ICMD_FADD:
	case ICMD_DADD:
	case ICMD_ISUB:
	case ICMD_LSUB:
	case ICMD_FSUB:
	case ICMD_DSUB:
	case ICMD_IMUL:
	case ICMD_LMUL:
	case ICMD_FMUL:
	case ICMD_DMUL:
	case ICMD_IDIV:
	case ICMD_LDIV:
	case ICMD_FDIV:
	case ICMD_DDIV:
	case ICMD_IREM:
	case ICMD_LREM:
	case ICMD_FREM:
	case ICMD_DREM:
	case ICMD_ISHL:
	case ICMD_LSHL:
	case ICMD_ISHR:
	case ICMD_LSHR:
	case ICMD_IUSHR:
	case ICMD_LUSHR:
	case ICMD_IAND:
	case ICMD_LAND:
	case ICMD_IOR:
	case ICMD_LOR:
	case ICMD_IXOR:
	case ICMD_LXOR:
	case ICMD_LCMP:
	case ICMD_FCMPL:
	case ICMD_FCMPG:
	case ICMD_DCMPL:
	case ICMD_DCMPG:
		SHOW_S1(iptr);
		SHOW_S2(iptr);
		SHOW_DST(iptr);
		break;

		/* binary/const INT */
	case ICMD_IADDCONST:
	case ICMD_ISUBCONST:
	case ICMD_IMULCONST:
	case ICMD_IMULPOW2:
	case ICMD_IDIVPOW2:
	case ICMD_IREMPOW2:
	case ICMD_IANDCONST:
	case ICMD_IORCONST:
	case ICMD_IXORCONST:
	case ICMD_ISHLCONST:
	case ICMD_ISHRCONST:
	case ICMD_IUSHRCONST:
	case ICMD_LSHLCONST:
	case ICMD_LSHRCONST:
	case ICMD_LUSHRCONST:
		SHOW_S1(iptr);
		SHOW_INT_CONST(iptr->sx.val.i);	
		SHOW_DST(iptr);
		break;

		/* ?ASTORECONST (trinary/const INT) */
	case ICMD_IASTORECONST:
	case ICMD_BASTORECONST:
	case ICMD_CASTORECONST:
	case ICMD_SASTORECONST:
		SHOW_S1(iptr);
		SHOW_S2(iptr);
		SHOW_INT_CONST(iptr->sx.s23.s3.constval);
		break;

		/* const INT */
	case ICMD_ICONST:
		SHOW_INT_CONST(iptr->sx.val.i);	
		SHOW_DST(iptr);
		break;

		/* binary/const LNG */
	case ICMD_LADDCONST:
	case ICMD_LSUBCONST:
	case ICMD_LMULCONST:
	case ICMD_LMULPOW2:
	case ICMD_LDIVPOW2:
	case ICMD_LREMPOW2:
	case ICMD_LANDCONST:
	case ICMD_LORCONST:
	case ICMD_LXORCONST:
		SHOW_S1(iptr);
		SHOW_LNG_CONST(iptr->sx.val.l);
		SHOW_DST(iptr);
		break;

		/* trinary/const LNG (<= pointer size) */
	case ICMD_LASTORECONST:
		SHOW_S1(iptr);
		SHOW_S2(iptr);
		SHOW_ADR_CONST(iptr->sx.s23.s3.constval);
		break;

		/* const LNG */
	case ICMD_LCONST:
		SHOW_LNG_CONST(iptr->sx.val.l);	
		SHOW_DST(iptr);
		break;

		/* const FLT */
	case ICMD_FCONST:
		SHOW_FLT_CONST(iptr->sx.val.f);	
		SHOW_DST(iptr);
		break;

		/* const DBL */
	case ICMD_DCONST:
		SHOW_DBL_CONST(iptr->sx.val.d);	
		SHOW_DST(iptr);
		break;

		/* const ADR */
	case ICMD_ACONST:
		if (iptr->flags.bits & INS_FLAG_CLASS) {
			SHOW_ADR_CONST(iptr->sx.val.anyptr);
			SHOW_CLASSREF_OR_CLASSINFO(iptr->sx.val.c);
		}
		else if (iptr->sx.val.anyptr == NULL) {
			printf("NULL ");
		}
		else {
			SHOW_ADR_CONST(iptr->sx.val.anyptr);
			SHOW_STRING(iptr->sx.val.stringconst);
		}
		SHOW_DST(iptr);
		break;

	case ICMD_AASTORECONST:
		SHOW_S1(iptr);
		SHOW_S2(iptr);
		printf("%p ", (void*) iptr->sx.s23.s3.constval);
		break;

	case ICMD_GETFIELD:        /* 1 -> 1 */
	case ICMD_PUTFIELD:        /* 2 -> 0 */
 	case ICMD_PUTSTATIC:       /* 1 -> 0 */
	case ICMD_GETSTATIC:       /* 0 -> 1 */
	case ICMD_PUTSTATICCONST:  /* 0 -> 0 */
	case ICMD_PUTFIELDCONST:   /* 1 -> 0 */
		if (opcode != ICMD_GETSTATIC && opcode != ICMD_PUTSTATICCONST) {
			SHOW_S1(iptr);
			if (opcode == ICMD_PUTFIELD) {
				SHOW_S2(iptr);
			}
		}
		INSTRUCTION_GET_FIELDREF(iptr, fmiref);
		SHOW_FIELD(fmiref);

		if (opcode == ICMD_GETSTATIC || opcode == ICMD_GETFIELD) {
			SHOW_DST(iptr);
		}
		break;

	case ICMD_IINC:
		SHOW_S1_LOCAL(iptr);
		SHOW_INT_CONST(iptr->sx.val.i);
		SHOW_DST_LOCAL(iptr);
		break;

	case ICMD_IASTORE:
	case ICMD_SASTORE:
	case ICMD_BASTORE:
	case ICMD_CASTORE:
	case ICMD_LASTORE:
	case ICMD_DASTORE:
	case ICMD_FASTORE:
	case ICMD_AASTORE:
		SHOW_S1(iptr);
		SHOW_S2(iptr);
		SHOW_S3(iptr);
		break;

	case ICMD_IALOAD:
	case ICMD_SALOAD:
	case ICMD_BALOAD:
	case ICMD_CALOAD:
	case ICMD_LALOAD:
	case ICMD_DALOAD:
	case ICMD_FALOAD:
	case ICMD_AALOAD:
		SHOW_S1(iptr);
		SHOW_S2(iptr);
		SHOW_DST(iptr);
		break;

	case ICMD_RET:
		SHOW_S1_LOCAL(iptr);
		if (stage >= SHOW_STACK) {
			printf(" ---> L%03d", iptr->dst.block->nr);
		}
		break;

	case ICMD_ILOAD:
	case ICMD_LLOAD:
	case ICMD_FLOAD:
	case ICMD_DLOAD:
	case ICMD_ALOAD:
		SHOW_S1_LOCAL(iptr);
		SHOW_DST(iptr);
		break;

	case ICMD_ISTORE:
	case ICMD_LSTORE:
	case ICMD_FSTORE:
	case ICMD_DSTORE:
	case ICMD_ASTORE:
		SHOW_S1(iptr);
		SHOW_DST_LOCAL(iptr);
		if (stage >= SHOW_STACK && iptr->sx.s23.s3.javaindex != UNUSED)
			printf(" (javaindex %d)", iptr->sx.s23.s3.javaindex);
		if (iptr->flags.bits & INS_FLAG_RETADDR) {
			printf(" (retaddr L%03d)", RETADDR_FROM_JAVALOCAL(iptr->sx.s23.s2.retaddrnr));
		}
		break;

	case ICMD_NEW:
		SHOW_DST(iptr);
		break;

	case ICMD_NEWARRAY:
		SHOW_DST(iptr);
		break;

	case ICMD_ANEWARRAY:
		SHOW_DST(iptr);
		break;

	case ICMD_MULTIANEWARRAY:
		if (stage >= SHOW_STACK) {
			argp = iptr->sx.s23.s2.args;
			i = iptr->s1.argcount;
			while (i--) {
				SHOW_VARIABLE(*(argp++));
			}
		}
		else {
			printf("argcount=%d ", iptr->s1.argcount);
		}
		SHOW_DST(iptr);
		break;

	case ICMD_CHECKCAST:
		SHOW_S1(iptr);
		putchar(' ');
		class_classref_or_classinfo_print(iptr->sx.s23.s3.c);
		SHOW_DST(iptr);
		break;

	case ICMD_INSTANCEOF:
		SHOW_S1(iptr);
		SHOW_DST(iptr);
		break;

	case ICMD_INLINE_START:
	case ICMD_INLINE_END:
	case ICMD_INLINE_BODY:
#if defined(ENABLE_INLINING)
		{
			insinfo_inline *ii = iptr->sx.s23.s3.inlineinfo;
			show_inline_info(jd, ii, opcode, stage);
		}
#endif
		break;

	case ICMD_BUILTIN:
		if (stage >= SHOW_STACK) {
			argp = iptr->sx.s23.s2.args;
			i = iptr->s1.argcount;
			while (i--) {
				if ((iptr->s1.argcount - 1 - i) == iptr->sx.s23.s3.bte->md->paramcount)
					printf(" pass-through: ");
				SHOW_VARIABLE(*(argp++));
			}
		}
		printf("%s ", iptr->sx.s23.s3.bte->cname);
		if (iptr->sx.s23.s3.bte->md->returntype.type != TYPE_VOID) {
			SHOW_DST(iptr);
		}
		break;

	case ICMD_INVOKEVIRTUAL:
	case ICMD_INVOKESPECIAL:
	case ICMD_INVOKESTATIC:
	case ICMD_INVOKEINTERFACE:
		if (stage >= SHOW_STACK) {
			methoddesc *md;
			INSTRUCTION_GET_METHODDESC(iptr, md);
			argp = iptr->sx.s23.s2.args;
			i = iptr->s1.argcount;
			while (i--) {
				if ((iptr->s1.argcount - 1 - i) == md->paramcount)
					printf(" pass-through: ");
				SHOW_VARIABLE(*(argp++));
			}
		}
		INSTRUCTION_GET_METHODREF(iptr, fmiref);
		method_methodref_print(fmiref);
		if (fmiref->parseddesc.md->returntype.type != TYPE_VOID) {
			putchar(' ');
			SHOW_DST(iptr);
		}
		break;

	case ICMD_IFEQ:
	case ICMD_IFNE:
	case ICMD_IFLT:
	case ICMD_IFGE:
	case ICMD_IFGT:
	case ICMD_IFLE:
		SHOW_S1(iptr);
		SHOW_INT_CONST(iptr->sx.val.i);	
		SHOW_TARGET(iptr->dst);
		break;

	case ICMD_IF_LEQ:
	case ICMD_IF_LNE:
	case ICMD_IF_LLT:
	case ICMD_IF_LGE:
	case ICMD_IF_LGT:
	case ICMD_IF_LLE:
		SHOW_S1(iptr);
		SHOW_LNG_CONST(iptr->sx.val.l);	
		SHOW_TARGET(iptr->dst);
		break;

	case ICMD_GOTO:
		SHOW_TARGET(iptr->dst);
		break;

	case ICMD_JSR:
		SHOW_TARGET(iptr->sx.s23.s3.jsrtarget);
		SHOW_DST(iptr);
		break;

	case ICMD_IFNULL:
	case ICMD_IFNONNULL:
		SHOW_S1(iptr);
		SHOW_TARGET(iptr->dst);
		break;

	case ICMD_IF_ICMPEQ:
	case ICMD_IF_ICMPNE:
	case ICMD_IF_ICMPLT:
	case ICMD_IF_ICMPGE:
	case ICMD_IF_ICMPGT:
	case ICMD_IF_ICMPLE:

	case ICMD_IF_LCMPEQ:
	case ICMD_IF_LCMPNE:
	case ICMD_IF_LCMPLT:
	case ICMD_IF_LCMPGE:
	case ICMD_IF_LCMPGT:
	case ICMD_IF_LCMPLE:

	case ICMD_IF_FCMPEQ:
	case ICMD_IF_FCMPNE:

	case ICMD_IF_FCMPL_LT:
	case ICMD_IF_FCMPL_GE:
	case ICMD_IF_FCMPL_GT:
	case ICMD_IF_FCMPL_LE:

	case ICMD_IF_FCMPG_LT:
	case ICMD_IF_FCMPG_GE:
	case ICMD_IF_FCMPG_GT:
	case ICMD_IF_FCMPG_LE:

	case ICMD_IF_DCMPEQ:
	case ICMD_IF_DCMPNE:

	case ICMD_IF_DCMPL_LT:
	case ICMD_IF_DCMPL_GE:
	case ICMD_IF_DCMPL_GT:
	case ICMD_IF_DCMPL_LE:

	case ICMD_IF_DCMPG_LT:
	case ICMD_IF_DCMPG_GE:
	case ICMD_IF_DCMPG_GT:
	case ICMD_IF_DCMPG_LE:

	case ICMD_IF_ACMPEQ:
	case ICMD_IF_ACMPNE:
		SHOW_S1(iptr);
		SHOW_S2(iptr);
		SHOW_TARGET(iptr->dst);
		break;

	case ICMD_TABLESWITCH:
		SHOW_S1(iptr);
		table = iptr->dst.table;

		i = iptr->sx.s23.s3.tablehigh - iptr->sx.s23.s2.tablelow + 1;

		printf("high=%d low=%d count=%d\n", iptr->sx.s23.s3.tablehigh, iptr->sx.s23.s2.tablelow, i);
		while (--i >= 0) {
			printf("\t\t%d --> ", (int) (table - iptr->dst.table));
			printf("L%03d\n", table->block->nr);
			table++;
		}

		break;

	case ICMD_LOOKUPSWITCH:
		SHOW_S1(iptr);

		printf("count=%d, default=L%03d\n",
			   iptr->sx.s23.s2.lookupcount,
			   iptr->sx.s23.s3.lookupdefault.block->nr);

		lookup = iptr->dst.lookup;
		i = iptr->sx.s23.s2.lookupcount;

		while (--i >= 0) {
			printf("\t\t%d --> L%03d\n",
				   lookup->value,
				   lookup->target.block->nr);
			lookup++;
		}
		break;

	case ICMD_FRETURN:
	case ICMD_IRETURN:
	case ICMD_DRETURN:
	case ICMD_LRETURN:
		SHOW_S1(iptr);
		break;

	case ICMD_ARETURN:
	case ICMD_ATHROW:
		SHOW_S1(iptr);
		if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
			/* XXX this needs more work */
#if 0
			unresolved_class_debug_dump(iptr->sx.s23.s2.uc, stdout);
#endif
		}
		break;

 	case ICMD_COPY:
 	case ICMD_MOVE:
		SHOW_S1(iptr);
		SHOW_DST(iptr);
		break;
	}
	fflush(stdout);
}
#endif /* !defined(NDEBUG) */


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
