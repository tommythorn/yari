extern void set_leds(unsigned v);
asm(".globl set_leds;.ent set_leds;set_leds: mtlo $4;jr $31; .end set_leds");


asm("	.section .startup_code");
asm("	.set noreorder");
asm("	.globl	_start");
asm("	.ent	_start");
asm("        .set    noreorder");
asm("        .set    noat");
asm("_start: li      $0, 0");
asm("        li      $1, 1");
asm("        li      $2, 2");
asm("        li      $3, 3");
asm("        li      $4, 4");
asm("        li      $5, 5");
asm("        li      $6, 6");
asm("        li      $7, 7");
asm("        li      $8, 8");
asm("        li      $9, 9");
asm("        li      $10, 10");
asm("        li      $11, 11");
asm("        li      $12, 12");
asm("        li      $13, 13");
asm("        li      $14, 14");
asm("        li      $15, 15");
asm("        li      $16, 16");
asm("        li      $17, 17");
asm("        li      $18, 18");
asm("        li      $19, 19");
asm("        li      $20, 20");
asm("        li      $21, 21");
asm("        li      $22, 22");
asm("        li      $23, 23");
asm("        li      $24, 24");
asm("        li      $25, 25");
asm("        li      $26, 26");
asm("        li      $27, 27");
asm("        li      $28, 28");
asm("        li      $29, 29");
asm("        li      $30, 30");
asm("        li      $31, 31");
asm("        .set    at");
asm("        la      $29,0x40080000");  /* Stack ends where gdb stub data begins. */
asm("        la      $28,_gp       ");  /* Globals pointer. */
asm("	     .set    reorder");
asm("	     .end    _start");

int
main()
{
        int led = 0;

        set_leds(0xF0);

        for (;;) {
                int i;
                for (i = 0; i < 1000000; ++i)
                        ;
                set_leds(led++);
        }
}
