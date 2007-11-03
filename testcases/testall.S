/*
 * Regression test
 */

#define SRAM 0x40000000

#define FAILURE la $2, .; mtlo $2; .word 0x48000000; 99: b 99b; nop
#define SUCCESS li $2, 0x87654321; mtlo $2; .word 0x48000000
#define EXPECT(r, v) li $30, v; beq $30,r,13f;nop;FAILURE;13:
#define EXPECT_ADDR(r, a) la $30, a; beq $30,r,13f;nop;FAILURE;13:
	.text
	.global	_start
	.ent	_start
_start:
	// Optimistic
        la      $6, 0xFF000000
        li	$7, '!'
        sb      $7, ($6)


        /* Check stalled delay slot (this assumes no forwarding of $2). */

	.set noreorder
	li	$2, 1
	b	$L1
	li	$2, 77
$L1:	EXPECT($2, 77)

        /* Check jumps and its delay slot. */
	j	$L3
	li	$2, 88
	FAILURE	/* shouldn't reach here */
$L3:	EXPECT($2, 88)

        /* Check jal and its delay slot. */
	jal	$L4
	li	$2, 99
$L3_cnt:FAILURE	/* shouldn't reach here */
$L4:	EXPECT_ADDR($31, $L3_cnt)
	EXPECT($2, 99)


/*
 * From here on, we assume jal works, and will use a more space
 * efficient results checking
 */
#undef EXPECT
#define EXPECT(r, v) li $29, v; jal check; move $30, r
#undef FAILURE
#define FAILURE jal fail

	/* Check half of beq */
	li	$2, 66
	li	$3, 66
	beq	$2,$3,$L5
	li	$4, 55
	FAILURE	/* shouldn't reach here */
$L5:	EXPECT($4,55)

	/* Check other half of beq */
	li	$2, 66
	li	$3, 65
	beq	$2,$3,1f
	li	$4, 54
	EXPECT($4,54)
	b	2f
	nop
1:	FAILURE	/* shouldn't reach here */
2:

	/* Check half of bne */
	li	$2, 666
	li	$3, 667
	bne	$2,$3,$L6
	li	$4, 555
	FAILURE	/* shouldn't reach here */
$L6:	EXPECT($4,555)

	/* check other half of bne */
	li	$2, 666
	li	$3, 666
	bne	$2,$3,1f
	li	$4, 553
	EXPECT($4,553)
	b	2f
	nop
1:	FAILURE	/* shouldn't reach here */
2:

	/* check blez, bgtz, addi, addiu, slti, sltiu, andi, ori xori lui [cp0
	   cp1 cp2] bbql bgtzl lb lh lwl lw lbu lhu lwr sb sh swl sw- swr
	*/

        nop
	li	$2, 0x89878685
	la	$3, SRAM
	sw	$2, ($3)
	lb	$4, ($3)
	EXPECT($4,0xFFFFFF89)

	lbu	$4, 3($3)
	EXPECT($4,0x85)

	lh	$4, 2($3)
        move    $5, $4   /* Test load-use hazard handling */
	EXPECT($5,0xFFFF8685)

	lw	$5, 4($3) /* Loads back-to-back */
	lhu	$4, ($3)
	EXPECT($4,0x00008987)


/* .... */


	/*
         * Test SRAM program execution by copying down a litle program.  Notice,
	 * we _can't_ copy from the bootprom as it may only exist in the in the
	 * preloaded instruction cache from which we can't read.
	 */

	b	1f
	nop

2:
	li	$4, 1728	// 0x240406c0
	jalr	$31 		// 0x03e0f809
	li	$4, 1729	// 0x240406c1
	nop			// 0x00000000

        /* Test I$ misses and also test bubbles in delay slot hazards */
1:	la	$10, SRAM + 30 * 4
	la	$5, 0x240406c0
	la	$2, 0x03e0f809
	la	$3, 0x240406c1
	la	$4, 0x00000000
	sw	$5,  0($10)
	sw	$2,  4($10)
	sw	$3,  8($10)
	/* This only works because we know that this line misses in I$ */
	jal	$10
	sw	$4, 12($10)
	EXPECT($4, 1729)


// Emit OkOk... a few times

	li      $5, 10
        li	$7, 79 // 'O'
        li	$9, 107 + 79 // 'k' + 'O'
loop:
/* Use this to exercize the hazard detection
        lw	$8, ($6)
loop2:  bne	$8, $0, loop2
        lw	$8, ($6) */


        lw	$8, ($6)
	nop

        bne	$8, $0, loop
	nop

        sb      $7, ($6)

        subu	$7, $9, $7

        bne	$5, $0, loop
	addi	$5, $5, -1

	SUCCESS


	li	$6, 1
1:
/* Do something on the frame buffer. */
	la	$4, 0x400E6A00
	la	$5, 0x400E6A00 + 4*(640*480/32 - 1)
2:
	sw	$5, 0($4)
	bne	$4, $5, 2b
	addi	$4, $4, 4

	la	$4, 0x400E6A00
	la	$5, 0x400E6A00 + 4*(640*480/32 - 1)
2:
	sw	$0, 0($4)
	bne	$4, $5, 2b
	addi	$4, $4, 4

fail:   move	$2, $31
	mtlo	$2
	.word	0x48000000
98:	b	98b
	nop

check:	bne	$29,$30,fail
        nop
        jr	$31
        nop
	.end	_start
