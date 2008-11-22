CFLAGS=-G0 -O
LDFLAGS=-Tyari.ld
LD=mips-elf-gcc

SIM=../../yarisim/sim

hilbert-host: hilbert.c
	cc -g hilbert.c -o hilbert-host

test-font1.mips: test-font1.c font1.c
	mips-elf-gcc $(LDFLAGS) $(CFLAGS) test-font1.c font1.c -o $@

all: hilbert.txt

%.txt: %.mips $(SIM)
	$(SIM) --tiny $< > $@


%.mips: %.c
	mips-elf-gcc $(LDFLAGS) $(CFLAGS) $< -o $@
