/*
 * Boot loader prom -> sram
 */

#define CNT $3
#define SRC $4
#define DST $5
#define t1 $9
#define t2 $10
#define t3 $11
#define t4 $12

	.globl _start
_start:
	la	SRC, 2f
	li	DST, 0x40000000
	li	CNT, 1024 # 4096 32-bit words
1:
	subu	CNT,CNT,1
	lw	t1,  0(SRC)
	lw	t2,  4(SRC)
	lw	t3,  8(SRC)
	lw	t4, 12(SRC)

	sw	t1,  0(DST)
	sw	t2,  4(DST)
	sw	t3,  8(DST)
	sw	t4, 12(DST)

	addu	SRC,SRC,16
	addu	DST,DST,16
	bne	CNT,$0,1b
	nop

	la	$28,_gp
	la	$29,0x40000000+1024*1024

	li	DST, 0x40000000
	j	DST
	nop
	.align	4
2:
