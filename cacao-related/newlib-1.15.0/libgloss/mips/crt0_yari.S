/*
 * crt0_yari.S -- Minimal startup file for YARI targets running tinymon
 *
 * Copyright (c) 1995, 1996, 1997, 2000 Red Hat, Inc.
 * Copyright (c) 2008 Tommy Thorn.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */

#include "regs.S"
        .text

        .set noreorder
        .globl  _start
        .set    noat
_start: li      $0, 0
        li      $1, 1
        li      $2, 2
        li      $3, 3
        li      $4, 4
        li      $5, 5
        li      $6, 6
        li      $7, 7
        li      $8, 8
        li      $9, 9
        li      $10, 10
        li      $11, 11
        li      $12, 12
        li      $13, 13
        li      $14, 14
        li      $15, 15
        li      $16, 16
        li      $17, 17
        li      $18, 18
        li      $19, 19
        li      $20, 20
        li      $21, 21
        li      $22, 22
        li      $23, 23
        li      $24, 24
        li      $25, 25
        li      $26, 26
        li      $27, 27
        li      $28, 28
        li      $29, 29
        li      $30, 30
        li      $31, 31

        la      $29,0x40100000 /* Stack ends where tinymon data begins. */
        la      $28,_gp        /* Globals pointer. */

        /* Clear the BSS segment. */
        la      $8, _fbss
        la      $9, _end

        /* 32-word align */
        subu    $4, $9, $8
        andi    $4, $4, 31 << 2
        beq     $4, $0, clear32_tail
        nop

clear1:
        addi    $4, $4, -4
        sw      $0, ($8)
        bne     $4, $0, clear1
        addi    $8, $8, 4

        beq     $8, $9, done_clearing
        nop

clear32:
        sw      $0,   ($8)
        sw      $0,  4($8)
        sw      $0,  8($8)
        sw      $0, 12($8)
        sw      $0, 16($8)
        sw      $0, 20($8)
        sw      $0, 24($8)
        sw      $0, 28($8)
        sw      $0, 32($8)
        sw      $0, 36($8)
        sw      $0, 40($8)
        sw      $0, 44($8)
        sw      $0, 48($8)
        sw      $0, 52($8)
        sw      $0, 56($8)
        sw      $0, 60($8)

        sw      $0, 64($8)
        sw      $0, 68($8)
        sw      $0, 72($8)
        sw      $0, 76($8)
        sw      $0, 80($8)
        sw      $0, 84($8)
        sw      $0, 88($8)
        sw      $0, 92($8)
        sw      $0, 96($8)
        sw      $0,100($8)
        sw      $0,104($8)
        sw      $0,108($8)
        sw      $0,112($8)
        sw      $0,116($8)
        sw      $0,120($8)
        sw      $0,124($8)
        addi    $8,$8,4*32

clear32_tail:
        bne     $8,$9,clear32
        nop

done_clearing:

        .set	at
        .set	reorder


    /*
     * initialize target specific stuff. Only execute these
     * functions it they exist.
     */
	.globl	hardware_init_hook .text
	.globl	software_init_hook .text
//	.globl	__do_global_dtors .text
	.globl	atexit .text
	.globl	exit .text
	.globl	init
	.ent	init
init:
	la		t9, hardware_init_hook	# init the hardware if needed
	beq		t9, zero, 6f
	nop
	jal		t9
	nop
6:
	la		t9, software_init_hook	# init the software if needed
	beq		t9, zero, 7f
	nop
	jal		t9
	nop
7:
//	la		a0, __do_global_dtors
//	jal		atexit
//	nop

#ifdef GCRT0
	.globl	_ftext
	.globl	_extext
	la		a0, _ftext
	la		a1, _etext
	jal		monstartup
	nop
#endif

        jal     __pre_main
        nop

        la      $5,argv
        jal     main
        li      $4,1

	# fall through to the "exit" routine
	jal		exit					# call libc exit to run the G++
									# destructors
	move	a0, v0					# pass through the exit code
	.end	init

/*
 * _exit -- Exit from the application. Normally we cause a user trap
 *          to return to the ROM monitor for another run. NOTE: This is
 *          the only other routine we provide in the crt0.o object, since
 *          it may be tied to the "_start" routine. It also allows
 *          executables that contain a complete world to be linked with
 *          just the crt0.o object.
 */
	.globl	_exit
	.ent	_exit
_exit:
7:
	b		7b						# loop back just in-case
	nop
	.end	_exit


        .rdata
        .align  2
argv:   .word   argv0
        .word   0
argv0:  .ascii  "prog\000"

/* EOF crt0.S */
