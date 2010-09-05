LD = mips-elf-ld
CC = mips-elf-gcc
#CFLAGS = -Wall -Wstrict-prototypes -fomit-frame-pointer -G 0 -mno-abicalls -fno-pic -mcpu=r3000 -mips1 -pipe
CFLAGS =-D_MIPS_SZLONG=32 -D_MIPS_SZPTR=32 -D_MIPS_SIM=1 -D__linux__ \
        -I. -Wall -Wstrict-prototypes -fomit-frame-pointer -G 0 \
        -mno-abicalls -fno-pic -mips1 -pipe \
	-Os

# -Wa,-alh,-L

OBJDUMP = mips-elf-objdump
STRIP = mips-elf-strip
CSRC=gdb-stub.c gdb-support.c kernmisc.c
SSRC=gdb-low.S
OBJ=$(patsubst %.c,%.o,$(CSRC)) $(patsubst %.S,%.o,$(SSRC))

gdbstub: ld.script.little $(OBJ)
	mips-elf-ld -Tprom.ld -o gdbstub.mips gdb-stub.o gdb-support.o gdb-low.o kernmisc.o
#	$(LD) -M -static -T prom.ld -o gdbstub.mips entry.o gdb-stub.o gdb-support.o gdb-low.o kernmisc.o
	$(OBJDUMP) -d -M reg-names=numeric gdbstub.mips > gdbstub.dis
	mips-elf-nm gdbstub.mips | sort > gdbstub.nm

%.s: %.c
	$(CC) $(CFLAGS) -S $< -o $@

asm: $(patsubst %.c,%.s,$(CSRC))

entry.o: entry.S
	$(CC) $(CFLAGS) -D__ASSEMBLY__ -c -o entry.o entry.S

gdb-low.o: gdb-low.S
	$(CC) $(CFLAGS) -D__ASSEMBLY__ -c -o gdb-low.o gdb-low.S

clean:
	-rm -f *.o *.mips *.dis *.s

realclean: clean
	-rm *~
