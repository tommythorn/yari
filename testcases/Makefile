#mips-elf-gcc -Tmymips.ld -msoft-float $< -o $@

HEAD=#  | head -2048
RUNMIPS=../swsim/runmips
OBJDUMP_FLAGS=-S -D -M reg-names=numeric
TINYMON=../firmware/tinymon-prom
PROG=showtime
VERB=--verb

all: cosim

working: testall.data testicache.data testeret-prom.data \
	gdbstub-prom.data showtime.mips letters.mips sramtester.mips

broken: stalls.mif \
     fail.mips test0.data test1.data testlwsw.data sieve.data sieve.mif  \
     serialout_nowait.mif muldiv.mif #  monitor.mif tests.data

try0: test0.mips $(RUNMIPS)
	$(RUNMIPS) --elf --simple --verb test0.mips

#%.o: %.c
#	mips-elf-gcc -msoft-float -Tmymips.ld -g -O $< -o $@

gdbstub-prom.mips: ../gdbstub/gdbstub.mips
	cp ../gdbstub/gdbstub.mips gdbstub-prom.mips

%-prom.mips: %.o
	mips-elf-ld -Tprom.ld $< -o $@

%-prom.data: %-prom.mips %-prom.mif $(RUNMIPS)
	$(RUNMIPS) --data $< > $@

bootloader.mips: bootloader.o
	mips-elf-ld -Tprom.ld $< -o $@
	-mips-elf-objdump $(OBJDUMP_FLAGS) $@ > $(basename $@).dis
	mips-elf-nm $@ | sort > $(basename $@).nm

bootloader.data: bootloader.mips $(RUNMIPS)
	$(RUNMIPS) --data $< > $@

spinscreen.mips: spinscreen.o
	mips-elf-ld -Tprom.ld $< -o $@
	-mips-elf-objdump $(OBJDUMP_FLAGS) $@ > $(basename $@).dis

sramtester.mips: sramtester.o
	mips-elf-ld -Tsram.ld $< -o $@
	-mips-elf-objdump $(OBJDUMP_FLAGS) $@ > $(basename $@).dis

spinscreen.data: spinscreen.mips $(RUNMIPS)
	$(RUNMIPS) --data $< > $@

testall.mips: testall.o
	mips-elf-ld -Tsram.ld testall.o -o $@

testall-prom.mips: testall.o
	mips-elf-ld -Tprom.ld $< -o $@

testall-prom.data: testall-prom.mips $(RUNMIPS)
	$(RUNMIPS) --data $< > $@

testicache.mips: testicache.o
	mips-elf-ld -Tsram.ld testicache.o -o $@

testicache-prom.mips: testicache.o
	mips-elf-ld -Tprom.ld $< -o $@

testicache-prom.data: testicache-prom.mips $(RUNMIPS)
	$(RUNMIPS) --data $< > $@

# cpp $< > $(basename $<).s DON'T DO THIS!!!  IT BREAKS TERRIBLY WHEN THE DIRECTORY IS SMB :-(
%.o: %.S
	gcc -E $< > $(basename $<)._s
	mips-elf-as $(basename $<)._s -o $@

# I think this might be causing trouble because the files are on a samba file system
#%.o: %.s
#	mips-elf-as $< -o $@

%.o: %.c
	mips-elf-gcc -DSTANDALONE -msoft-float -g -O -c $< -o $@

.PRECIOUS: %.dis %.mips %.data %.mif

%.dis: %.mips
	-mips-elf-objdump $(OBJDUMP_FLAGS) $< > $@

crt0.o: crt0.S

%.mips: %.o Makefile crt0.o
	mips-elf-ld -Tsram.ld crt0.o $< -o $@
	-mips-elf-objdump -d -M reg-names=numeric $@ > $(basename $@).dis
	-mips-elf-nm $@ | sort > $(basename $@).nm

%.mif: %.mips $(RUNMIPS)
	-$(RUNMIPS) --mif $< > $@

%.data: %.mips %.dis %.mif bootloader.data $(RUNMIPS)
	-(cat bootloader.data; $(RUNMIPS) --data $< $(HEAD)) > $@

#%.data: %.mips %.dis %.mif bootloader.data
#	-$(RUNMIPS) --data $< $(HEAD) > $@

show: test0.mips
	mips-elf-objdump -D test0.mips

muldiv.mips: muldiv.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c muldiv.c
	mips-elf-ld -Tstandalone.ld muldiv.o -o muldiv.mips

sieve.mips: sieve.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c sieve.c
	mips-elf-ld -Tsram.ld sieve.o -o sieve.mips

hello.mips: hello.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c hello.c
	mips-elf-ld -Tstandalone.ld hello.o -o hello.mips

hello2.mips: hello2.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c hello2.c
	mips-elf-ld -Tstandalone.ld hello2.o -o hello2.mips

testshifts.mips: testshifts.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c testshifts.c
	mips-elf-ld -Tstandalone.ld testshifts.o -o testshifts.mips

stalls.mips: stalls.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c stalls.c
	mips-elf-ld -Tstandalone.ld stalls.o -o stalls.mips

testvga.mips: testvga.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c testvga.c
	mips-elf-ld -Tstandalone.ld testvga.o -o testvga.mips

testifsram.mips: testifsram.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c testifsram.c
	mips-elf-ld -Tstandalone.ld testifsram.o -o testifsram.mips

sermon.mips: sermon.c
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c sermon.c
	mips-elf-ld -Tstandalone.ld sermon.o -o sermon.mips

#memorytester.mips: memorytester.c
#	mips-elf-gcc -DSTANDALONE -msoft-float -O -c memorytester.c
#	mips-elf-ld -Tstandalone.ld memorytester.o -o memorytester.mips

#$(RUNMIPS) --simp sieve.mips
#make sieve.data
#$(RUNMIPS) --simp --verb sieve.mips > sieve.trace

%.txt: %.mips $(RUNMIPS)
	$(RUNMIPS) --tiny $< > $@

hw2.mips: hw2.S
	mips-elf-gcc -DSTANDALONE -msoft-float -O -c hw2.S
	mips-elf-ld -Tsram.ld hw2.o -o hw2.mips

rtlsim: $(PROG).txt $(TINYMON).data
	cp $< ../rtl/Icarus/input.txt
	make -C ../rtl/Icarus TESTPROG=../$(TINYMON).data simulate

isasim: $(PROG).txt $(RUNMIPS) $(TINYMON).mips
	make -C ../swsim FLAGS="-i ../testcases/$(PROG).txt "$(VERB) TESTPROG=$(TINYMON).mips run

cosim:  $(PROG).txt $(RUNMIPS) $(TINYMON).mips $(TINYMON).data
	cp $< ../rtl/Icarus/input.txt
	make -C ../rtl/Icarus TESTPROG=../$(TINYMON).data simulate | \
	make -C ../swsim FLAGS="-i ../testcases/$(PROG).txt --cosim "$(VERB) TESTPROG=$(TINYMON).mips run

$(RUNMIPS):
	make -C ../swsim

$(TINYMON).mips:
	make -C ../firmware

$(TINYMON).data:
	make -C ../firmware

clean:
	-rm *.o *._s *.mips *.txt *.dis *.nm

realclean: clean
	-rm *~ a.out *.mif *.data *.s
