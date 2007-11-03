RUNYARI=../yarisim/sim
OBJDUMP_FLAGS=-S -D -M reg-names=numeric

.PRECIOUS: %.dis %.mips %.data %.mif %.mips
.SUFFIX: .mipsel-o .mipsel-s .mif .data

promote: firmware
	cp tinymon-prom.mif ../rtl/initmem.mif
	cp tinymon-prom.data ../rtl/initmem.data

$(RUNYARI):
	make -C ../yarisim sim

firmware: tinymon-prom.mif tinymon-prom.data tinymon-prom.dis tinymon-prom.nm

yarisim: tinymon-prom.mips tinymon-prom.dis
	make -C ../yarisim FLAGS='-i ../firmware/test.txt --firm --verb --regwrites' TESTPROG=../firmware/tinymon-prom.mips run

rtlsim: tinymon-prom.data tinymon-prom.dis promote
	cp test.txt ../rtl/Icarus/input.txt
	make -C ../rtl/target/Icarus TESTPROG=../../firmware/tinymon-prom.data simulate


# Ugh, an unfortunate quick workaround for my broken xtool installation
%.mipsel-s: %.c
	mips-elf-gcc -D_mips_ -msoft-float -g -O -S $< -o $@

%.mipsel-o: %.mipsel-s
	mips-elf-as $< -o $@

%-prom.mips: %.mipsel-o prom.ld
	mips-elf-ld -Tprom.ld $< -o $@

%-prom.mif: %-prom.mips $(RUNYARI)
	-$(RUNYARI) --mif $< > $@ || rm $@

%-prom.data: %-prom.mips %-prom.mif $(RUNYARI)
	$(RUNYARI) --data $< > $@ || rm $@

%.dis: %.mips
	-mips-elf-objdump $(OBJDUMP_FLAGS) $< > $@

%.nm: %.mips
	-mips-elf-nm -a $< > $@

hosttest:
	cc -g tinymon.c -o tinymon
	./tinymon < test.txt

clean:
	-rm *.mipsel-o *.mipsel-s *.mif *.mips *.data *.dis *.mif *.mips tinymon

realclean: clean
	-rm *~
