	.section .startup_code
	.set noreorder
	.globl	_start
	.ent	_start
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

        la    $29,0x40080000 /* Stack ends where gdb stub data begins. */
        la    $28,_gp        /* Globals pointer. */

        /* Clear the BSS segment. */
       	la      $8, _fbss
	la	$9, _end
clear:  sw	$0, ($8)
        bne	$8,$9,clear
        addi    $8,$8,4

        jal   main
        nop
	.set reorder
	.end	_start
