/*	src/vm/jit/m68k/md.c

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

   $Id: arch.h 5330 2006-09-05 18:43:12Z edwin $

*/
#include "config.h"

#include <assert.h>

#include "md-os.h"

#include "vm/types.h"
#include "vm/jit/codegen-common.h"
#include "vm/jit/md.h"

#include "offsets.h"
#include "vm/vm.h"
#include "vmcore/class.h"
#include "vmcore/linker.h"
#include "vmcore/method.h"
#include "mm/memory.h"
#include "vm/jit/asmpart.h"
/*
 *	As a sanity measuremnt we assert the offset.h values in here as m68k gets
 *	crosscompiled for sure and noone thinks of offset.h wen changing compile flags
 *	and subtile bugs will result...
 *
 *	m68k uses the trap instruction for hardware exceptions, need to register
 *	according signal handler
 */
void md_init(void) 
{
	assert(sizeof(vm_arg) == sizevmarg);
	assert(OFFSET(java_objectheader, vftbl) == offobjvftbl);
	assert(OFFSET(vftbl_t, baseval) == offbaseval);
	assert(OFFSET(vftbl_t, diffval) == offdiffval);
	assert(OFFSET(vm_arg, type) == offvmargtype);
	assert(OFFSET(vm_arg, data) == offvmargdata);
	assert(OFFSET(castinfo, super_baseval) == offcast_super_baseval);
	assert(OFFSET(castinfo, super_diffval) == offcast_super_diffval);
	assert(OFFSET(castinfo, sub_baseval) == offcast_sub_baseval);

#if defined(ENABLE_REPLACEMENT)
	assert(sizeof(executionstate_t) = sizeexecutionstate);
	assert(OFFSET(executionstate_t, pc) == offes_pc);
	assert(OFFSET(executionstate_t, sp) == offes_sp);
	assert(OFFSET(executionstate_t, pv) == offes_pv);
	assert(OFFSET(executionstate_t, intregs) == offes_intregs);
	assert(OFFSET(executionstate_t, fltregs) == offes_fltregs);
#endif

#ifdef __LINUX__
	md_init_linux();
#endif
}

/* md_codegen_get_pv_from_pc ***************************************************

   On this architecture just a wrapper function to
   codegen_get_pv_from_pc.

*******************************************************************************/
u1* md_codegen_get_pv_from_pc(u1 *ra) 
{ 
	u1 *pv;
	pv = codegen_get_pv_from_pc(ra);

	return pv;
}

/* md_get_method_patch_address *************************************************
 
   Gets the patch address of the currently compiled method. Has to be 
   extracted from the load instructions which lead to the jump.

from asmpart.S (asm_vm_call_method):
84:   2879 0000 0000  moveal 0 <asm_vm_call_method-0x34>,%a4
8a:   4e94            jsr %a4@


from invokestatic / invokespecial
0x40290882:   247c 4029 03b4    moveal #1076429748,%a2
0x40290888:   4e92              jsr %a2@

from invokevirtual
0x40297eca:   266a 0000         moveal %a2@(0),%a3
0x40297ece:   246b 002c         moveal %a3@(44),%a2
0x40297ed2:   4e92              jsr %a2@



*******************************************************************************/

u1* md_get_method_patch_address(u1 *ra, stackframeinfo *sfi, u1 *mptr) 
{
	u1 * pa;
	s2   offset;

	if (*((u2*)(ra - 2)) == 0x4e94)	{		/* jsr %a4@ */
		if (*((u2*)(ra - 6)) == 0x286b)	{
			/* found an invokevirtual */
			/* get offset of load instruction 246b XXXX */
			offset = *((s2*)(ra - 4));
			pa = mptr + offset;			/* mptr contains the magic we want */
		} else	{
			/* we had a moveal XXX, %a3 which is a 3 word opcode */
			/* 2679 0000 0000 */
			assert(*(u2*)(ra - 8) == 0x2879);		/* moveal */
			pa = *((u4*)(ra - 6));				/* another indirection ! */
		}
	} else if (*((u2*)(ra - 2)) == 0x4e92)	{		/* jsr %a2@ */
		if (*(u2*)(ra - 8) == 0x247c)	{
			/* found a invokestatic/invokespecial */
			pa = ((u4*)(ra - 6));			/* no indirection ! */
		} else {
			assert(0);
		}
	} else {
		assert(0);
	}

	return pa;
}

/* XXX i can't find a definition of cacheflush in any installed header files but i can find the symbol in libc */
/* lets extract the signature from the assembler code*/
/*
    000e7158 <cacheflush>:
    e7158:       707b            moveq #123,%d0
    e715a:       2f04            movel %d4,%sp@-
    e715c:       282f 0014       movel %sp@(20),%d4			arg 
    e7160:       2243            moveal %d3,%a1
    e7162:       262f 0010       movel %sp@(16),%d3			arg 
    e7166:       2042            moveal %d2,%a0
    e7168:       242f 000c       movel %sp@(12),%d2			arg 
    e716c:       222f 0008       movel %sp@(8),%d1			arg 
    e7170:       4e40            trap #0				traps into system i guess
    e7172:       2408            movel %a0,%d2
    e7174:       2609            movel %a1,%d3
    e7176:       281f            movel %sp@+,%d4
    e7178:       223c ffff f001  movel #-4095,%d1
    e717e:       b081            cmpl %d1,%d0
    e7180:       6402            bccs e7184 <cacheflush+0x2c>
    e7182:       4e75            rts
    e7184:       4480            negl %d0
    e7186:       2f00            movel %d0,%sp@-
    e7188:       61ff fff3 82e2  bsrl 1f46c <D_MAX_EXP+0x1ec6d>
    e718e:       209f            movel %sp@+,%a0@
    e7190:       70ff            moveq #-1,%d0
    e7192:       2040            moveal %d0,%a0
    e7194:       4e75            rts
    e7196:       4e75            rts
									*/

/* seems to have 4 arguments */
/* best guess: it is this syscall */
/* asmlinkage int sys_cacheflush (unsigned long addr, int scope, int cache, unsigned long len) */
/* kernel 2.6.10 with freescale patches (the one I develop against) needs a patch of */
/* arch/m68k/kernel/sys_m68k.c(sys_cacheflush) */
/* evil hack: */
/*
void DcacheFlushInvalidateCacheBlock(void *start, unsigned long size);
void IcacheInvalidateCacheBlock(void *start, unsigned long size);

asmlinkage int
sys_cacheflush (unsigned long addr, int scope, int cache, unsigned long len)
{
	lock_kernel();
	DcacheFlushInvalidateCacheBlock(addr, len);
	IcacheInvalidateCacheBlock(addr, len);
	unlock_kernel();
	return 0;
}
*/
extern int cacheflush(unsigned long addr, int scope, int cache, unsigned long len);

#include "asm/cachectl.h"	/* found more traces of the cacheflush function */
#include "errno.h"

void md_cacheflush(u1 *addr, s4 nbytes)  { cacheflush(addr, FLUSH_SCOPE_PAGE, FLUSH_CACHE_BOTH, nbytes); }
void md_dcacheflush(u1 *addr, s4 nbytes) { cacheflush(addr, FLUSH_SCOPE_PAGE, FLUSH_CACHE_DATA, nbytes); }
void md_icacheflush(u1* addr, s4 nbytes) { cacheflush(addr, FLUSH_SCOPE_LINE, FLUSH_CACHE_INSN, nbytes); }

/* md_stacktrace_get_returnaddress *********************************************

   Returns the return address of the current stackframe, specified by
   the passed stack pointer and the stack frame size.

*******************************************************************************/
u1* md_stacktrace_get_returnaddress(u1* sp, u4 framesize) 
{ 
	/* return address is above stackpointer */
	u1 *ra = *((u1**)(sp + framesize));
	
	/* XXX: This helps for now, but it's a ugly hack
	 * the problem _may_ be: the link instruction is used
	 * by some gcc generated code, and we get an additional word
	 * on the stack, the old framepointer. Its address is somewhere
	 * near sp, but that all depends the code generated by the compiler.
	 * I'm unsure about a clean solution.
	 */
	#if 0
	if (!(ra > 0x40000000 && ra < 0x80000000))      {
	        ra = *((u1**)(sp + framesize + 4));
	}
	#endif
	/* assert(ra > 0x40000000 && ra < 0x80000000);
	printf("XXXXXX=%x\n", ra);
	 */
	return ra;
}


void md_codegen_patch_branch(void) { assert(0); }


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
