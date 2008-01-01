/*
 * gdb-low.S
 *
 * gdb-low.S contains the low-level trap handler for the GDB stub.
 *
 * Copyright (C) 2005 Tommy Thorn
 * Copyright (C) 1995 Andreas Busse
 *
 */

#include <linux/sys.h>

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/mipsconfig.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/gdb-stub.h>

        .globl _start

	.section .init

	/* 0xBFC0_0000 */

_cold_reset:
_soft_reset:
_nmi:
_start:
	.set	noat
	.set 	noreorder
        li	$0, 0
	la      $1, _fdata // In theory ... _fbss
	la	$2, _end
clear:  sw	$0, ($1)
        addi    $1,$1,4
        bne	$1,$2,clear
        nop
        break

	.org	_cold_reset + 0x200 /* 0xBFC0_0200 */
_tbl_refill:
        break

	.org    _tbl_refill +  0x80 /* 0xBFC0_0280 */
_xtbl_refill:
        break

	.org	_xtbl_refill + 0x100 /* 0xBFC0_0380 */
_other_exceptions:

/*
	CLI
        la	sp, __init_end
        jal	trap_low
        nop

*/


/*
 * The low level trap handler
 */

		NESTED(trap_low, GDB_FR_SIZE, sp)
 		.set	noat
		.set 	noreorder

		mfc0	k0,CP0_STATUS
		sll	k0,3     		/* extract cu0 bit */
		bltz	k0,1f
		move	k1,sp

		/*
		 * Called from user mode, new stack
		 */
#if 0
		lui	k1,%hi(kernelsp)
		lw	k1,%lo(kernelsp)(k1)
#else
		la	k1,__init_end
#endif
1:
		move	k0,sp
		subu	sp,k1,GDB_FR_SIZE
		sw	k0,GDB_FR_REG29(sp)
		sw	v0,GDB_FR_REG2(sp)

/*
 * First save the CP0 and special registers
 */

		mfc0	v0,CP0_STATUS
		sw	v0,GDB_FR_STATUS(sp)
		mfc0	v0,CP0_CAUSE
		sw	v0,GDB_FR_CAUSE(sp)
		mfc0	v0,CP0_EPC
		sw	v0,GDB_FR_EPC(sp)
		mfc0	v0,CP0_BADVADDR
		sw	v0,GDB_FR_BADVADDR(sp)
		mfhi	v0
		sw	v0,GDB_FR_HI(sp)
		mflo	v0
		sw	v0,GDB_FR_LO(sp)

/*
 * Now the integer registers
 */

		sw	zero,GDB_FR_REG0(sp)		/* I know... */
		sw	$1,GDB_FR_REG1(sp)
		/* v0 already saved */
		sw	v1,GDB_FR_REG3(sp)
		sw	a0,GDB_FR_REG4(sp)
		sw	a1,GDB_FR_REG5(sp)
		sw	a2,GDB_FR_REG6(sp)
		sw	a3,GDB_FR_REG7(sp)
		sw	t0,GDB_FR_REG8(sp)
		sw	t1,GDB_FR_REG9(sp)
		sw	t2,GDB_FR_REG10(sp)
		sw	t3,GDB_FR_REG11(sp)
		sw	t4,GDB_FR_REG12(sp)
		sw	t5,GDB_FR_REG13(sp)
		sw	t6,GDB_FR_REG14(sp)
		sw	t7,GDB_FR_REG15(sp)
		sw	s0,GDB_FR_REG16(sp)
		sw	s1,GDB_FR_REG17(sp)
		sw	s2,GDB_FR_REG18(sp)
		sw	s3,GDB_FR_REG19(sp)
		sw	s4,GDB_FR_REG20(sp)
		sw	s5,GDB_FR_REG21(sp)
		sw	s6,GDB_FR_REG22(sp)
		sw	s7,GDB_FR_REG23(sp)
		sw	t8,GDB_FR_REG24(sp)
		sw	t9,GDB_FR_REG25(sp)
		sw	k0,GDB_FR_REG26(sp)
		sw	k1,GDB_FR_REG27(sp)
		sw	gp,GDB_FR_REG28(sp)
		/* sp already saved */
		sw	fp,GDB_FR_REG30(sp)
		sw	ra,GDB_FR_REG31(sp)

		CLI				/* disable interrupts */

/*
 * Current stack frame ptr
 */

		sw	sp,GDB_FR_FRP(sp)

/*
 * CP0 registers (R4000/R4400 unused registers skipped)
 */

		mfc0	v0,CP0_INDEX
		sw	v0,GDB_FR_CP0_INDEX(sp)
		mfc0	v0,CP0_RANDOM
		sw	v0,GDB_FR_CP0_RANDOM(sp)
		mfc0	v0,CP0_ENTRYLO0
		sw	v0,GDB_FR_CP0_ENTRYLO0(sp)
		mfc0	v0,CP0_ENTRYLO1
		sw	v0,GDB_FR_CP0_ENTRYLO1(sp)
		mfc0	v0,CP0_CONTEXT
		sw	v0,GDB_FR_CP0_CONTEXT(sp)
		mfc0	v0,CP0_PAGEMASK
		sw	v0,GDB_FR_CP0_PAGEMASK(sp)
		mfc0	v0,CP0_WIRED
		sw	v0,GDB_FR_CP0_WIRED(sp)
		mfc0	v0,CP0_ENTRYHI
		sw	v0,GDB_FR_CP0_ENTRYHI(sp)
		mfc0	v0,CP0_PRID
		sw	v0,GDB_FR_CP0_PRID(sp)

		.set	at

/*
 * Continue with the higher level handler
 */

		move	a0,sp

		jal	handle_exception
		nop

/*
 * Restore all writable registers, in reverse order
 */

		.set	noat

		lw	v0,GDB_FR_CP0_ENTRYHI(sp)
		lw	v1,GDB_FR_CP0_WIRED(sp)
		mtc0	v0,CP0_ENTRYHI
		mtc0	v1,CP0_WIRED
		lw	v0,GDB_FR_CP0_PAGEMASK(sp)
		lw	v1,GDB_FR_CP0_ENTRYLO1(sp)
		mtc0	v0,CP0_PAGEMASK
		mtc0	v1,CP0_ENTRYLO1
		lw	v0,GDB_FR_CP0_ENTRYLO0(sp)
		lw	v1,GDB_FR_CP0_INDEX(sp)
		mtc0	v0,CP0_ENTRYLO0
		lw	v0,GDB_FR_CP0_CONTEXT(sp)
		mtc0	v1,CP0_INDEX
		mtc0	v0,CP0_CONTEXT


/*
 * Now the CP0 and integer registers
 */

		mfc0	t0,CP0_STATUS
		ori	t0,0x1f
		xori	t0,0x1f
		mtc0	t0,CP0_STATUS

		lw	v0,GDB_FR_STATUS(sp)
		lw	v1,GDB_FR_EPC(sp)
		mtc0	v0,CP0_STATUS
		mtc0	v1,CP0_EPC
		lw	v0,GDB_FR_HI(sp)
		lw	v1,GDB_FR_LO(sp)
		mthi	v0
		mtlo	v1
		lw	ra,GDB_FR_REG31(sp)
		lw	fp,GDB_FR_REG30(sp)
		lw	gp,GDB_FR_REG28(sp)
		lw	k1,GDB_FR_REG27(sp)
		lw	k0,GDB_FR_REG26(sp)
		lw	t9,GDB_FR_REG25(sp)
		lw	t8,GDB_FR_REG24(sp)
		lw	s7,GDB_FR_REG23(sp)
		lw	s6,GDB_FR_REG22(sp)
		lw	s5,GDB_FR_REG21(sp)
		lw	s4,GDB_FR_REG20(sp)
		lw	s3,GDB_FR_REG19(sp)
		lw	s2,GDB_FR_REG18(sp)
		lw	s1,GDB_FR_REG17(sp)
		lw	s0,GDB_FR_REG16(sp)
		lw	t7,GDB_FR_REG15(sp)
		lw	t6,GDB_FR_REG14(sp)
		lw	t5,GDB_FR_REG13(sp)
		lw	t4,GDB_FR_REG12(sp)
		lw	t3,GDB_FR_REG11(sp)
		lw	t2,GDB_FR_REG10(sp)
		lw	t1,GDB_FR_REG9(sp)
		lw	t0,GDB_FR_REG8(sp)
		lw	a3,GDB_FR_REG7(sp)
		lw	a2,GDB_FR_REG6(sp)
		lw	a1,GDB_FR_REG5(sp)
		lw	a0,GDB_FR_REG4(sp)
		lw	v1,GDB_FR_REG3(sp)
		lw	v0,GDB_FR_REG2(sp)
		lw	$1,GDB_FR_REG1(sp)
		lw	sp,GDB_FR_REG29(sp)		/* Deallocate stack */

		.set	mips3
		eret
		.set	mips0
		.set	at
		.set	reorder
		END(trap_low)
