	.text
	.globl	_start
	.ent	_start
#define P	$5
#define R	$11
#define C	$4
#define C2	$12
#define t1	$9
#define t2	$10

#if 0
# define WORDS	640*480/32
# define SHORT   50000
# define LONG	1000000
# define DELAY(x) li C2,x; 7: subu C2,C2,1; bne C2,$0,7b
#else
# define WORDS	10
# define SHORT   1
# define LONG	 1
# define DELAY(x)
#endif

_start: li	R, 1

	/* First clear the frame buffer to a known state. */
	li	C, WORDS
	li	P, 0x400E6A00
clear:	sub	C, C, 1
	sw	$0, (P)
	DELAY(SHORT)
	add	P, P, 4
	bnez	C, clear

restart:li	P, 0x400E6A00
	li	C, WORDS

loop:	subu	C, C, 1
	lw	t1, 0(P)
	/*	xor	t1, P, t1 */
	add	t1, t1, 1
	sw	t1, 0(P)
	DELAY(SHORT)
	add	P, P, 4
	bne	C, $0, loop

	/* Delay */
	li	C, LONG
delay:	subu	C, C, 1
	DELAY(0)
	bne	C, $0, delay
	b	restart
	.end	_start
