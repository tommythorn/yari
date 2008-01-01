CC=mips-elf-gcc
CFLAGS=-msoft-float -O2 -g
OBJ=mymips.o cma101.o fstat.o getpid.o isatty.o kill.o lseek.o print.o putnum.o stat.o syscalls.o unlink.o vr4300.o
TARGET=/opt/mips-elf/mips-elf/lib/soft-float/libmymips.a

libmymips.a: $(OBJ)
	-rm libmymips.a
	mips-elf-ar qv libmymips.a $(OBJ)
	mips-elf-ranlib libmymips.a
	sudo cp $@ $(TARGET)

mymips.o: mymips.c

