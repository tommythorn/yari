CFLAGS=-G0 -O
LDFLAGS=-Tyari.ld
LD=mips-elf-gcc

SIM=../../yarisim/sim

hilbert-host: hilbert.c
	cc -g hilbert.c -o hilbert-host

test-font1.mips: test-font1.c font-fixed-6x13.c
	mips-elf-gcc $(LDFLAGS) $(CFLAGS) $^ -o $@

all: hilbert.txt

%.txt: %.mips $(SIM)
	$(SIM) --tiny $< > $@


%.mips: %.c
	mips-elf-gcc $(LDFLAGS) $(CFLAGS) $< -o $@
