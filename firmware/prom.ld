/*
 * This linker script was based on the nullmon.ld from the libgloss
 * (newlib) distribution.
 *
 * YARI assumes to have memory from 0x8000000 onwards.  Typically 1MiB or
 * 16MiB if SDRAM is used.
 *
 * The memory map:
 *
 * #0000_0000 - #7FFF_FFFF Unmapped
 * #4000_0000 - #400F_FFFF SRAM (1MiB SRAM)
 *     #4000_0000 - #400E_0000 (896KiB, user space)
 *     #400E_6A00 - #400E_FFFF (38.4KiB normal place for the framebuffer)
 *     #400F_0000 - #400F_FFFF (64KiB, reserved for the GDB stub monitor)
 * #4010_0000 - #BFBF_FFFF Unmapped
 *
 * #BFC0_0000 - #BFC0_3FFF PROM (16KiB)
 * #BFC0_4000 - #FEFF_FFFF Unmapped
 *
 * #FF00_0000 - #FF??_???? IO space
 *
 * Handwired entrypoints:
 * #BFC00000 Reset vector
 * #BFC00380 Exception handling
 *
 */

/* The following TEXT start address leaves space for the monitor
   workspace. */

ENTRY(_start)
/*STARTUP(crt0.o)*/
OUTPUT_ARCH("mips:3000")
OUTPUT_FORMAT("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
/*GROUP(-lc -lmymips -lgcc)*/
SEARCH_DIR(.)
__DYNAMIC  =  0;

/*
 * Allocate the stack to be at the top of memory, since the stack
 * grows down
 */
PROVIDE (__stack = 0);
/* PROVIDE (__global = 0); */

/*
 * Initalize some symbols to be zero so we can reference them in the
 * crt0 without core dumping. These functions are all optional, but
 * we do this so we can have our crt0 always use them if they exist. 
 * This is so BSPs work better when using the crt0 installed with gcc.
 * We have to initalize them twice, so we multiple object file
 * formats, as some prepend an underscore.
 */
PROVIDE (hardware_exit_hook = 0);
PROVIDE (hardware_hazard_hook = 0);
PROVIDE (hardware_init_hook = 0);
PROVIDE (software_init_hook = 0);

SECTIONS
{
  . = 0xBFC00000;
  .text : {
     _ftext = . ;
    *(.init)
     eprol  =  .;
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t*)
    *(.mips16.fn.*)
    *(.mips16.call.*)
    PROVIDE (__runtime_reloc_start = .);
    *(.rel.sdata)
    PROVIDE (__runtime_reloc_stop = .);
    *(.fini)
     etext  =  .;
     _etext  =  .;
  }

  . = .;
  .rodata : {
    *(.rdata)
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r*)
  }

  . = 0x400E0000;
   _fdata = ALIGN(16);
  .data : {
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d*)
  }
  . = ALIGN(8);
  _gp = . /* + 0x8000*/;
  __global = _gp;
  .lit8 : {
    *(.lit8)
  }
  .lit4 : {
    *(.lit4)
  }
  .sdata : {
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s*)
  }
  . = ALIGN(4);
   edata  =  .;
   _edata  =  .;
   _fbss = .;
  .sbss : {
    *(.sbss)
    *(.scommon)
  }
  .bss : {
    _bss_start = . ;
    *(.bss)
    *(COMMON)
  }
   . = ALIGN(64) ;
   end = .;
   _end = .;
}
