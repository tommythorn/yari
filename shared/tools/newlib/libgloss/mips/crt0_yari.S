        .section .startup_code
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

        la    $29,0x400E0000 /* Stack ends where tinymon data begins. */
        la    $28,_gp        /* Globals pointer. */

        /* Clear the BSS segment. */
        la      $8, _fbss
        la      $9, _end

        /* 32-word align */
        subu    $4, $9, $8
        andi    $4, $4, 31 << 2
        beq     $4, $0, clear32_tail

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
        jal     __pre_main
        nop

        li      $4,1
        la      $5,argv
        jal     main
        nop

        .globl  _exit
_exit:
        jal     __post_main
        nop

        li $2, 0x87654321
        mtlo $2;
        .word 0x48000000
        break

        .rdata
        .align  2
argv:   .word   argv0
        .word   0
argv0:  .ascii  "prog\000"



