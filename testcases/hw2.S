	.text
	.align	2
	.globl	main
	.ent	main
main:
        la	$2, 0xBFC00000
        jr	$2
        nop
        .end    main
