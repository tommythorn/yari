/*
 * Test framework
 *
 * Test termination is defined by executing a jump to itself.
 * Tests succeeded if $7 == 0x1729 and failed otherwise.
 *
 * fail -- One of the simplest test possible, it always fails.
 */

.text
	.globl	_start
	.set	noat
	.set	noreorder
	.set	nomacro
_start:
here:	j	here
	ori	$0, $0, 0x0000
	ori	$0, $0, 0x0000
