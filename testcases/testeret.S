/*
 * Coprocessor 0 register names
 */
#define CP0_INDEX $0
#define CP0_RANDOM $1
#define CP0_ENTRYLO0 $2
#define CP0_ENTRYLO1 $3
#define CP0_CONF $3
#define CP0_CONTEXT $4
#define CP0_PAGEMASK $5
#define CP0_WIRED $6
#define CP0_INFO $7
#define CP0_BADVADDR $8
#define CP0_COUNT $9
#define CP0_ENTRYHI $10
#define CP0_COMPARE $11
#define CP0_STATUS $12
#define CP0_CAUSE $13
#define CP0_EPC $14
#define CP0_PRID $15
#define CP0_CONFIG $16
#define CP0_LLADDR $17
#define CP0_WATCHLO $18
#define CP0_WATCHHI $19
#define CP0_XCONTEXT $20
#define CP0_FRAMEMASK $21
#define CP0_DIAGNOSTIC $22
#define CP0_DEBUG $23
#define CP0_DEPC $24
#define CP0_PERFORMANCE $25
#define CP0_ECC $26
#define CP0_CACHEERR $27
#define CP0_TAGLO $28
#define CP0_TAGHI $29
#define CP0_ERROREPC $30
#define CP0_DESAVE $31

#define t0 $3

/*
 * Test the eret instructions
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
	.set noreorder

        /* Test EPC return.  Set ERL=1 and ErrorPC = good1. */
        mfc0	t0,CP0_STATUS
        ori	t0,0x1f
        xori	t0,0x1b 	/* Lower 5 bits are KSU:2, ERL, EXL, and IE. */
        mtc0	t0,CP0_STATUS
        la	t0,good1
        mtc0	t0,CP0_ERROREPC
        la	t0,fail1
        mtc0	t0,CP0_EPC

        li	$6, 23
	.set	mips3
        eret
        .set	mips0
        li	$6, 27
        li	$6, 27

good1:  EXPECT($6, 23)

        /* Test ErrorPC return.  Set ERL=0 and EPC = good2. */
        mfc0	t0,CP0_STATUS
        ori	t0,0x1f
        xori	t0,0x1f
        mtc0	t0,CP0_STATUS
        la	t0,fail2
        mtc0	t0,CP0_ERROREPC
        la	t0,good2
        mtc0	t0,CP0_EPC

        li	$6, 23
	.set	mips3
        eret
	.set	mips0
        li	$6, 27
        li	$6, 27

good2:  EXPECT($6, 23)
	SUCCESS

fail1:  FAILURE
        nop
        nop
fail2:  FAILURE
	.end	_start
