#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "elf.h"
#include <getopt.h>
#include "mips32.h"
#include "runmips.h"
#include <ctype.h>

int n_call = 0;
int n_cycle = 0;
int n_issue = 0;
int n_stall = 0;

unsigned text_start = ~0;
unsigned text_size  = 0;
unsigned mif_size = 16384;
int serial_wait = 0;
int rs232in_fd, rs232out_fd;
unsigned rs232in_data    = 0;
unsigned char rs232in_cnt = 0;
unsigned rs232in_pending = 0; // last one is simulation only

unsigned nsections = 0;
unsigned section_start[99];
unsigned section_size[99];
static int text_segments = 0;

#define H(x) (endian_is_big ? ntohs(x) : x)
#define W(x) (endian_is_big ? ntohl(x) : x)

void service_rs232(void)
{
        /*
         * Check the serial port and update rs232in_data and
         * rs232in_cnt accordingly.
         */
        if (rs232in_fd >= 0 && !rs232in_pending) {
                char ch;
                int count = read(rs232in_fd, &ch, 1);
                if (count == 1) {
                        rs232in_pending = 3; // Minimum 2
                        rs232in_data = ch;
                }
        }
}

void initialize_memory(void)
{
        const unsigned megabyte  = 1024 * 1024;

        // See mymips.ld
        // ensure_mapped_memory_range(0, megabyte); // XXX Don't !
        ensure_mapped_memory_range(0x40000000, megabyte);
        // ensure_mapped_memory_range(0x90000000, megabyte);
        // ensure_mapped_memory_range(0x80000000-megabyte+1, megabyte-1);
        ensure_mapped_memory_range(0xBFC00000, megabyte); // Really only 16KiB

        unsigned p;
        int k;

        // Mimick real memory
        for (p = 0x40000000, k = megabyte; k > 0; p += 4, k -= 4)
                store(p, 0xe2e1e2e1, 4);

        for (p = 0x40000000, k = megabyte; k > 0; p += 4, k -= 4)
                assert(load(p, 4, 0) == 0xe2e1e2e1);
}

void ensure_mapped_memory_range(unsigned addr, unsigned len)
{
        unsigned seg;

        if (enable_disass) {
                printf("Ensure mapped [%08x; %08x]\n", addr, addr + len - 1);
        }

        // Split it up
        while (segment(addr) != segment(addr + len - 1)) {
                ensure_mapped_memory_range(addr, (1 << OFFSETBITS) - offset(addr));
                addr += (1 << OFFSETBITS) - offset(addr);
                assert(len >= (1 << OFFSETBITS) - offset(addr));
                len  -= (1 << OFFSETBITS) - offset(addr);
        }

        seg = segment(addr);
        if (!addr_mapped(addr + len - 1)) {
                memory_segment_size[seg] = 1 + offset(addr + len - 1);
                memory_segment[seg] =
                        realloc(memory_segment[seg], memory_segment_size[seg]);

                if (enable_disass) {
                        printf("Segment %2d virt [%08x; %08x] phys [%p; %p]\n",
                               seg,
                               addr, addr + len - 1,
                               memory_segment[seg],
                               memory_segment[seg] + memory_segment_size[seg] - 1);
                }

                // Make sure it's good
                *(char*)(memory_segment[seg] + memory_segment_size[seg] - 1) = 0;
        }

        assert(addr_mapped(addr + len - 1));
}

void exception(char *kind)
{
        printf("Exception caught: %s\n", kind);
        exit(1);
}

void loadsection(FILE *f, unsigned f_offset, unsigned f_len, unsigned m_addr,
                 unsigned m_len)
{
        void *phys;

//        m_len += 0x1000; // XXX Fudge factor?  hw writes outside BBS

        ensure_mapped_memory_range(m_addr, m_len);

        section_start[nsections]  = m_addr;
        section_size[nsections++] = m_len;
        assert(nsections < sizeof section_start / sizeof(unsigned));

        /*
         * We clear memory so that BBS doesn't need special
         * initialization
         */
        phys = addr2phys(m_addr);
        memset(phys, 0, m_len);

        fseek(f, f_offset, SEEK_SET);
        assert(segment(m_addr) == segment(m_addr + m_len)); // Handle that case later
        fread(addr2phys(m_addr), f_len, 1, f);
}

void readelf(char *name)
{
        static int memory_is_initialized = 0;

        Elf32_Ehdr ehdr;
        Elf32_Phdr *ph;
        int i;
        unsigned phentsize;

        FILE *f = fopen(name, "r");

        if (!f)
                fatal("Can't open %s\n", name);

        if (fread(&ehdr, sizeof(ehdr), 1, f) != 1)
                fatal("Can't read the ELF header of %s", name);

        if (strncmp((char *) ehdr.e_ident, ELFMAG, SELFMAG))
                fatal("%s is not an ELF file\n", name);

        endian_is_big = ehdr.e_ident[EI_DATA] == 2;

        /*
         * We can't initialize memory until we know the endian,
         * otherwise the RTL view of memory will not match.
         */
        if (!memory_is_initialized) {
                initialize_memory();
                memory_is_initialized = 1;
        }



        if (H(ehdr.e_type) != ET_EXEC)
                fatal("%s is not an executable\n", name);

        if (H(ehdr.e_machine) != EM_MIPS)
                fatal("%s is not a MIPS executable\n", name);

        if (enable_verb_elf) {
                printf("%s:\n", name);
                printf("%sendian\n", endian_is_big ? "big" : "little");
                printf("Entry:             %08x\n", W(ehdr.e_entry)); /* Entry point virtual address */
                printf("Proc Flags:        %08x\n", W(ehdr.e_flags)); /* Processor-specific flags */
                printf("Phdr.tbl entry cnt % 8d\n", H(ehdr.e_phnum));    /*Program header table entry count */
                printf("Shdr.tbl entry cnt % 8d\n", H(ehdr.e_shnum));    /*Section header table entry count */
                printf("Shdr.str tbl idx   % 8d\n", H(ehdr.e_shstrndx)); /*Section header string table index */
        }
        program_entry = W(ehdr.e_entry);

        if (H(ehdr.e_ehsize) != sizeof(ehdr)) {
                fprintf(stderr, "Oops, I can't handle this Elf header size");
                exit(1);
        }

        phentsize = H(ehdr.e_phentsize);

        if (H(ehdr.e_shentsize) != sizeof(Elf32_Shdr)) {
                fprintf(stderr, "Oops, I can't handle this Elf section header size");
                exit(1);
        }

        // Allocate program headers
        ph = malloc(sizeof(*ph) * H(ehdr.e_phnum));

        for (i = 0; i < H(ehdr.e_phnum); ++i) {
                fseek(f, W(ehdr.e_phoff) + i * phentsize, SEEK_SET);
                if (fread(&ph[i], sizeof(ph[i]), 1, f) != 1)
                        fatal("Can't read a program header");

                if (enable_verb_elf) {
                        printf("\nProgram header #%d (%lx)\n", i, ftell(f));
                        printf(" type             %08x\n", W(ph[i].p_type));
                        printf(" filesz           %08x\n", W(ph[i].p_filesz));
                        printf(" offset           %08x\n", W(ph[i].p_offset));
                        printf(" vaddr            %08x\n", W(ph[i].p_vaddr));
                        printf(" paddr            %08x\n", W(ph[i].p_paddr));
                        printf(" memsz            %08x\n", W(ph[i].p_memsz));
                        printf(" flags            %08x\n", W(ph[i].p_flags));
                        printf(" align            %08x\n", W(ph[i].p_align));
                }

                if (enable_firmware_mode &&
                    (W(ph[i].p_vaddr) & 0xFFFF0000) != 0xBFC00000)
                        // we simulate the bare hardware, we cannot preload
                        // the sram. Thus we'll ignore everything that isn't
                        // for BFC0XXXX
                        continue;

                if (W(ph[i].p_type) == PT_LOAD) {
                        ensure_mapped_memory_range(W(ph[i].p_vaddr),
                                                   W(ph[i].p_memsz));
                        if (W(ph[i].p_filesz)) {
                                fprintf(stderr, "Loading section [%08x; %08x]\n",
                                        W(ph[i].p_vaddr), W(ph[i].p_memsz));
                                loadsection(f, W(ph[i].p_offset), W(ph[i].p_filesz),
                                            W(ph[i].p_vaddr), W(ph[i].p_memsz));
                        }
                }

                if (W(ph[i].p_flags) & 1) {
                        // XXX I _think_ this means executable, but I
                        // haven't checked!
                        text_segments++;
                        text_start = W(ph[i].p_vaddr);
                        text_size  = W(ph[i].p_memsz);
                }
        }

        if (enable_verb_elf) {
                printf("\n");

                fseek(f, W(ehdr.e_shoff), SEEK_SET);
                for (i = 0; i < H(ehdr.e_shnum); ++i) {
                        Elf32_Shdr sh;

                        if (fread(&sh, sizeof(sh), 1, f) != 1)
                                fatal("Can't read a section header");

                        printf("\nSection header #%d (%lx)\n", i, ftell(f));
                        printf(" name            %08x\n", W(sh.sh_name));
                        printf(" type            %08x\n", W(sh.sh_type));
                        printf(" flags           %08x\n", W(sh.sh_flags));
                        printf(" addr            %08x\n", W(sh.sh_addr));
                        printf(" offset          %08x\n", W(sh.sh_offset));
                        printf(" size            %08x\n", W(sh.sh_size));
                        printf(" link            %08x\n", W(sh.sh_link));
                        printf(" info            %08x\n", W(sh.sh_info));
                        printf(" addralign       %08x\n", W(sh.sh_addralign));
                        printf(" entsize         %08x\n", W(sh.sh_entsize));
                }
                printf(" (now at %lx)\n", ftell(f));
        }
}

void dis_load_store(char *buf, char *name, inst_t i)
{
        if (i.i.imm)
                sprintf(buf, "%-6s$%d,%d($%d)", name, i.r.rt, i.i.imm, i.r.rs);
        else
                sprintf(buf, "%-6s$%d,($%d)", name, i.r.rt, i.r.rs);
}

void dis_load_store_cp1(char *buf, char *name, inst_t i)
{
        if (i.i.imm)
                sprintf(buf, "%-6s$f%d,%d($%d)", name, i.r.rt, i.i.imm, i.r.rs);
        else
                sprintf(buf, "%-6s$f%d,($%d)", name, i.r.rt, i.r.rs);
}

void disass(unsigned pc, inst_t i)
{
        char buf[999];
        unsigned npc = pc + 4;

        switch (i.j.opcode) {
        default: fatal("Unknown instruction opcode 0d%d", i.j.opcode);
        case REG:
                switch (i.r.funct) {
                default:   sprintf(buf,"??0x%x?? $%d,$%d,%d", i.r.funct, i.r.rd, i.r.rt, i.r.sa);
                           break;
                case SLL:
                  if (i.r.rd | i.r.rt | i.r.sa)
                    sprintf(buf,"%-6s$%d,$%d,%d", "sll", i.r.rd, i.r.rt, i.r.sa);
                  else
                    sprintf(buf,"nop");
                  break;
                case SRL:  sprintf(buf,"%-6s$%d,$%d,%d","srl", i.r.rd, i.r.rt, i.r.sa); break;
                case SRA:  sprintf(buf,"%-6s$%d,$%d,%d","sra", i.r.rd, i.r.rt, i.r.sa); break;
                case SLLV: sprintf(buf,"%-6s$%d,$%d,$%d","sllv", i.r.rd, i.r.rt, i.r.rs); break;
                case SRLV: sprintf(buf,"%-6s$%d,$%d,$%d","srlv", i.r.rd, i.r.rt, i.r.rs); break;
                case SRAV: sprintf(buf,"%-6s$%d,$%d,$%d","srav", i.r.rd, i.r.rt, i.r.rs); break;
                case JR:   sprintf(buf,"%-6s$%d","jr", i.r.rs); break;
                case JALR:
                        if (i.r.rd == 31)
                                sprintf(buf,"%-6s$%d","jalr", i.r.rs);
                        else
                                sprintf(buf,"%-6s$%d,$%d","jalr", i.r.rs, i.r.rd);
                        break;

                case SYSCALL: sprintf(buf,"%-6s","syscall"); break;

                case MFHI: sprintf(buf,"%-6s$%d","mfhi", i.r.rd); break;
                case MTHI: sprintf(buf,"%-6s$%d","mthi", i.r.rs); break;
                case MFLO: sprintf(buf,"%-6s$%d","mflo", i.r.rd); break;
                case MTLO: sprintf(buf,"%-6s$%d","mtlo", i.r.rs); break;
                case MULT: sprintf(buf,"%-6s$%d,$%d","mult", i.r.rs, i.r.rt); break;
                case MULTU:sprintf(buf,"%-6s$%d,$%d","multu", i.r.rs, i.r.rt); break;
                case DIV:  sprintf(buf,"%-6s$%d,$%d","div", i.r.rs, i.r.rt); break;
                case DIVU: sprintf(buf,"%-6s$%d,$%d","divu", i.r.rs, i.r.rt); break;
                case ADD:  sprintf(buf,"%-6s$%d,$%d,$%d","add", i.r.rd, i.r.rs, i.r.rt); break;
                case ADDU:
                        if (i.r.rt)
                                sprintf(buf,"%-6s$%d,$%d,$%d","addu", i.r.rd, i.r.rs, i.r.rt);
                        else
                                sprintf(buf,"%-6s$%d,$%d","move", i.r.rd, i.r.rs);
                        break;
                case SUB:  sprintf(buf,"%-6s$%d,$%d,$%d","sub", i.r.rd, i.r.rs, i.r.rt); break;
                case SUBU: sprintf(buf,"%-6s$%d,$%d,$%d","subu", i.r.rd, i.r.rs, i.r.rt); break;
                case AND:  sprintf(buf,"%-6s$%d,$%d,$%d","and", i.r.rd, i.r.rs, i.r.rt); break;
                case OR:
                  if (i.r.rt)
                          sprintf(buf,"%-6s$%d,$%d,$%d","or",i.r.rd, i.r.rs, i.r.rt);
                  else
                          sprintf(buf,"%-6s$%d,$%d","move",i.r.rd, i.r.rs);
                  break;
                case XOR:  sprintf(buf,"%-6s$%d,$%d,$%d","xor", i.r.rd, i.r.rs, i.r.rt); break;
                case NOR:  sprintf(buf,"%-6s$%d,$%d,$%d","nor", i.r.rd, i.r.rs, i.r.rt); break;
                case SLT:  sprintf(buf,"%-6s$%d,$%d,$%d","slt", i.r.rd, i.r.rs, i.r.rt); break;
                case SLTU: sprintf(buf,"%-6s$%d,$%d,$%d","sltu", i.r.rd, i.r.rs, i.r.rt); break;
                case BREAK:sprintf(buf,"%-6s","break"); break;
                }
                break;
        case REGIMM:
                switch (i.r.rt) {
                default: fatal("REGIMM rt=%d", i.r.rt);
                case BLTZ:  sprintf(buf,"%-6s$%d,0x%08x","bltz", i.r.rs, npc + (i.i.imm << 2)); break;
                case BGEZ:  sprintf(buf,"%-6s$%d,0x%08x","bgez", i.r.rs, npc + (i.i.imm << 2)); break;
                case BLTZAL:sprintf(buf,"%-6s$%d,0x%08x","bltzal", i.r.rs, npc + (i.i.imm << 2)); break;
                case BGEZAL:
                        if (i.r.rs)
                                sprintf(buf,"%-6s$%d,0x%08x","bgezal", i.r.rs, npc + (i.i.imm << 2));
                        else
                                sprintf(buf,"%-6s0x%08x","bal", npc + (i.i.imm << 2));
                        break;
                }
                break;
        case J:    sprintf(buf,"%-6s0x%08x","j", (npc & ~((1<<28)-1)) | (i.j.offset << 2)); break;
        case JAL:  sprintf(buf,"%-6s0x%08x","jal", (npc & ~((1<<28)-1)) | (i.j.offset << 2)); break;
        case BEQ:
                if (i.r.rs | i.r.rt)
                        sprintf(buf,"%-6s$%d,$%d,0x%08x","beq", i.r.rs, i.r.rt, npc + (i.i.imm << 2));
                else
                        sprintf(buf,"%-6s0x%08x","b", npc + (i.i.imm << 2));
                break;
        case BNE:  sprintf(buf,"%-6s$%d,$%d,0x%08x","bne", i.r.rs, i.r.rt, npc + (i.i.imm << 2)); break;
        case BLEZ: sprintf(buf,"%-6s$%d,$%d,0x%08x","blez", i.r.rs, i.r.rt, npc + (i.i.imm << 2)); break;
        case BGTZ: sprintf(buf,"%-6s$%d,$%d,0x%08x","bgtz", i.r.rs, i.r.rt, npc + (i.i.imm << 2)); break;
        case ADDI: sprintf(buf,"%-6s$%d,$%d,%d","addi", i.r.rt, i.r.rs, i.i.imm); break;
        case ADDIU:
                if (i.r.rs)
                        sprintf(buf,"%-6s$%d,$%d,%d","addiu", i.r.rt, i.r.rs, i.i.imm);
                else
                        sprintf(buf,"%-6s$%d,%d","li", i.r.rt, i.i.imm);
                break;
        case SLTI: sprintf(buf,"%-6s$%d,$%d,%d","slti", i.r.rt, i.r.rs, i.i.imm); break;
        case SLTIU:sprintf(buf,"%-6s$%d,$%d,%d","sltiu", i.r.rt, i.r.rs, i.i.imm); break;
        case ANDI: sprintf(buf,"%-6s$%d,$%d,0x%04x","andi", i.r.rt, i.r.rs, i.u.imm); break;
        case ORI:  sprintf(buf,"%-6s$%d,$%d,0x%04x","ori", i.r.rt, i.r.rs, i.u.imm); break;
        case XORI: sprintf(buf,"%-6s$%d,$%d,0x%04x","xori", i.r.rt, i.r.rs, i.u.imm); break;
        case LUI:  sprintf(buf,"%-6s$%d,0x%x","lui", i.r.rt, i.u.imm); break;
        case CP0:
                /* Two possible formats */
                if (i.r.rs & 0x10) {
                        /* C1 format */
                        sprintf(buf,
                                i.r.funct == C0_TLBR  ? "tlbr" :
                                i.r.funct == C0_TLBWI ? "tlbwi" :
                                i.r.funct == C0_TLBWR ? "tlbwr" :
                                i.r.funct == C0_TLBP  ? "tlbp" :
                                i.r.funct == C0_ERET  ? "eret" :
                                i.r.funct == C0_DERET ? "deret" :
                                i.r.funct == C0_WAIT  ? "wait" :
                                "???");
                } else {
                        /* Regular r format with i.r.rs[3] ? MT : MF */
                        sprintf(buf,
                                "%-6s$%d,$%d (sel %d, rs %d)",
                                i.r.rs & 4 ? "mtc0" : "mfc0",
                                i.r.rt, i.r.rd,
                                i.r.funct,
                                i.r.rs);
                }
/*
40826000        mtc0    v0,$12
40806800        mtc0    zero,$13
40826000        mtc0    v0,$12
408f4000        mtc0    t7,$8
*/
                break;
        case CP1:  sprintf(buf,"%-6s", "cp1"); break;
        case CP2:  sprintf(buf,"%-6s", "cp2"); break;
        case CP3:  sprintf(buf,"%-6s", "cp3"); break;
        case BEQL: sprintf(buf,"%-6s","bbql"); break;
        case LB:   dis_load_store(buf, "lb", i); break;
        case LH:   dis_load_store(buf, "lh", i); break;
        case LW:   dis_load_store(buf, "lw", i); break;
        case LBU:  dis_load_store(buf, "lbu", i); break;
        case LHU:  dis_load_store(buf, "lhu", i); break;
        case SB:   dis_load_store(buf, "sb", i); break;
        case SH:   dis_load_store(buf, "sh", i); break;
        case SW:   dis_load_store(buf, "sw", i); break;
        case LWC1: dis_load_store_cp1(buf, "lwc1", i); break;
        case SWC1: dis_load_store_cp1(buf, "swc1", i); break;
        case LDC1: dis_load_store_cp1(buf, "ldc1", i); break;
        case SDC1: dis_load_store_cp1(buf, "sdc1", i); break;
        }
        printf("%-24s", buf);
}

unsigned load(unsigned a,  // IN: address
              int c,       // IN: count of bytes to load
              int fetch)   // IN: instruction or data?
{
        unsigned res;

        /*
         * Handle special load devices.
         * So far we only have a serial output port.
         */
        if (!fetch && (a >> 4) == 0xFF00000) {
                switch ((a >> 2) & 0xF) {
                case 0: // rs232out_busy
                        if (serial_wait) {
                                serial_wait--;  // Not quite accurate, but ..
                                res = 1;
                        } else
                                res = 0;
                        break;
                case 1: res = rs232in_data & 255;
                        if (enable_disass) {
                                fprintf(stderr, "\nSERIAL INPUT '%c' (%d)\n",
                                        res, res);
                        }
                        rs232in_pending = 0;
                        break;
                case 2:
                        if (rs232in_pending > 1) {
                                if (--rs232in_pending == 1)
                                        ++rs232in_cnt;
                        }
                        res = rs232in_cnt; break;

                case 3:
                        // TSC
                        res = 0;
                        break;

                default:
                        fatal("Unknown register 0x%08x\n", a);
                }

                service_rs232();

                // BIG ENDIAN (XXX)
                switch (c) {
                case 1: return (res >> (~a & 3) *  8) &   0xFF;
                case 2: return (res >> (~a & 1) * 16) & 0xFFFF;
                case 4: return res;
                default: assert(0);
                }
        }

        if (!(addr_mapped(a) && addr_mapped(a + c - 1))) {
                // fatal("Loading from outside memory 0x%08x\n", a);
                fprintf(stderr, "Loading from outside memory 0x%08x\n", a);
                return 0;
        }

        if (!fetch && (a & 0xFFF00000) == 0xBFC00000) {
                fatal("\tloading from boot PROM 0x%08x\n", a);
                return 0;
        }

        if (a & (c - 1))
                fatal("Unaligned load from 0x%08x\n", a);

        // BIG ENDIAN

        switch (c) {
        case 1:
                res = *(u_int8_t *)addr2phys(a);
                break;
        case 2:
                res = H(*(u_int16_t*)addr2phys(a));
                break;
        case 4:
                res = W(*(u_int32_t*)addr2phys(a));
                break;
        default: assert(0);
        }

        if (0 && enable_disass && !fetch)
                switch (c) {
                case 1: printf("\t\t\t\t\t%02x <- [%08x]\n",
                               res, a);
                        break;
                case 2: printf("\t\t\t\t\t%04x <- [%08x]\n",
                               res, a);
                        break;
                case 4: printf("\t\t\t\t\t%08x <- [%08x]\n",
                               res, a);
                        break;
                }

        return res;
}

void store(unsigned a, unsigned v, int c)
{
        // XXX debug
        void *phys;

        /*
         * Handle special load devices.
         * So far we only have a serial output port.
         */
        if (a == 0xFF000000) {
                unsigned char ch = v;
                serial_wait = 0;
                if (enable_disass) {
                        fprintf(stderr, "\nSERIAL OUTPUT '%c' (%d)\n",
                                ch, ch);
                } else {
                        printf("%c", v);
                        fflush(stdout);
                }
                if (rs232out_fd >= 0) {
                        // fputc(ch, stderr);
                        // fprintf(stderr, "SERIAL OUT '%c' (%d)\n", ch, ch);
                        write(rs232out_fd, &ch, 1);
                }
                return;
        }

        if (!(addr_mapped(a) && addr_mapped(a + c - 1)))
                fatal("\tstoring outside memory 0x%08x\n", a);

        if ((a & 0xFFF00000) == 0xBFC00000) {
                fatal("\tstoring to boot PROM 0x%08x\n", a);
        }

        if (a & (c - 1))
                fatal("\tUnaligned store at 0x%08x\n", a);

        if (0 && enable_disass)
                switch (c) {
                case 1: printf("\t\t\t\t\t%02x -> [%08x] (was %02x)\n",
                               v, a, *(u_int8_t *)addr2phys(a));
                        break;
                case 2: printf("\t\t\t\t\t%04x -> [%08x] (was %04x)\n",
                               v, a, ntohs(*(u_int16_t*)addr2phys(a)));
                        break;
                case 4: printf("\t\t\t\t\t%08x -> [%08x] (was %08x)\n",
                               v, a, ntohl(*(u_int32_t*)addr2phys(a)));
                        break;
                }

        switch (c) {
        case 1: *(u_int8_t *)(phys = addr2phys(a)) = v; break;
        case 2: *(u_int16_t*)(phys = addr2phys(a)) = H(v); break;
        case 4: *(u_int32_t*)(phys = addr2phys(a)) = W(v); break;
        default: exit(1);
        }
}

static unsigned char
chksum(unsigned x)
{
        return (x & 255) + ((x >> 8) & 255) + ((x >> 16) & 255) + ((x >> 24) & 255);
}

void dump(char kind)
{
        unsigned p = text_start;
        int i;

//      assert(text_start == 0x80000000);

        // This is used to dump to "prom" and doesn't need to be very general
        assert(text_segments == 1);


        if (kind == 'm') {
                printf("-- yarisim generated Memory Initialization File (.mif)\n"
                       "\n"
                       "WIDTH=32;\n"
                       "DEPTH=%d;\n"
                       "\n"
                       "ADDRESS_RADIX=HEX;\n"
                       "DATA_RADIX=HEX;\n"
                       "\n"
                       "CONTENT BEGIN\n",
                       mif_size / 4);
        }

        for (i = 0; p < text_start + text_size; p += 4, ++i) {
                unsigned data = load(p, 4, 1);

                if (kind == 'd')
                        printf("%08X\n", data);
                else if (kind == 'm') {
                        printf("\t%08x : %08x;\n", i, data);
                } else {
                        unsigned char checksum;
                        checksum = 4 + chksum(i) + chksum(data);
                        printf(":04%04x00%08x%02x",
                               i, data, checksum);
                }
        }

        if (kind == 'm') {
                printf("END;\n");
        }


}

static void tinymon_cmd(unsigned char cmd, unsigned val)
{
        unsigned char chk = cmd;
        int i;
        int leading_zero = 1;

        putchar(cmd);
        for (i = 0; i < 8; ++i) {
                unsigned char c = "0123456789abcdef"[val >> 28];

                if ((val >> 28) != 0 || !leading_zero || i == 7) {
                        leading_zero = 0;

                        chk += c;
                        putchar(c);
                }
                val <<= 4;
        }
        putchar(' ');
        putchar("0123456789abcdef"[chk >> 4]);
        putchar("0123456789abcdef"[chk & 15]);
        putchar('\r');
        putchar('\n');
}

void dump_tinymon(void)
{
        unsigned p, k;

        tinymon_cmd('c', 0);

        for (k = 0; k < nsections; ++k) {
                tinymon_cmd('l', section_start[k]);
                unsigned end = section_start[k] + section_size[k];
                for (p = section_start[k]; p < end; p += 4)
                        tinymon_cmd('w', load(p, 4, 1));
        }

        tinymon_cmd('e', program_entry);
}

// Local Variables:
// mode: C
// c-style-variables-are-local-p: t
// c-file-style: "linux"
// End:
