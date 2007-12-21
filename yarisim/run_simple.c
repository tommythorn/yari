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

unsigned coverage[64+64+32];
char *inst_name[64+64+32] = {
        // ROOT MAP
        // "REG", "REGIMM",
        [2] = "J", "JAL", "BEQ", "BNE", "BLEZ", "BGTZ",
        [8]="ADDI", "ADDIU", "SLTI", "SLTIU", "ANDI", "ORI", "XORI", "LUI",
        [0x10]="CP0", "CP1",
        // "CP2", "CP3",
        [0x14]="BEQL", "BNEL", "BLEZL", "BGTZL",

        [0x20]="LB",
        [0x21]="LH",
//        [0x22]="LWL",
        [0x23]="LW",
        [0x24]="LBU",
//        [0x25]="LHU",
//        [0x26]="LWR",
        [0x28]="SB",
        [0x29]="SH",
//        [0x2a]="SWL",
        [0x2b]="SW",
//        [0x2e]="SWR", "CACHE",
        //[0x30]="LL", "LWC1", "LWC2", "PREF", "LDC1", "LCD2",
        //[0x38]="SC", "SWC1", "SWC2", [0x3d]="SDC1", "SCD2",

        // REG MAP
        [64] = "SLL",
        [64+2] = "SRL", "SRA", "SLLV", [64+6] = "SRLV", "SRAV",
        [64+8] = "JR", "JALR", [64 + 0xC] = "SYSCALL", "BREAK",
        [64+0x10] = "MFHI", "MTHI", "MFLO", "MTLO",
        [64+0x18] = "MULT", "MULTU", "DIV", "DIVU",
        [64+0x20] = "ADD", "ADDU", "SUB", "SUBU", "AND", "OR", "XOR", "NOR",
        [64+0x2a] = "SLT", "SLTU",

        // REGIMM MAP
        [128+0] = "BLTZ",
        [128+1] = "BGEZ",
        [128+0x10] = "BLTZAL",
        [128+0x11] = "BGEZAL"

};

#define UNTESTED() ({ if (tested[__LINE__]++ == 0) printf(__FILE__ ":%d: not tested\n", __LINE__); })
#define TESTED()

#define KEEP_LINES 50
#define RTL_MAX_LINE 200
char last[KEEP_LINES][RTL_MAX_LINE];
unsigned last_p = 0;

int get_rtl_commit(unsigned *cycle, unsigned *pc, unsigned *wbr, unsigned *wbv)
{
        char buf[RTL_MAX_LINE];
        unsigned time = ~0;

        for (;;) {
                int r;
                char *p = fgets(buf, sizeof buf, stdin);
                if (!p)
                        return 0;

                strncpy(last[last_p++], buf, sizeof last[0]);
                if (last_p == KEEP_LINES)
                        last_p = 0;

                r = sscanf(buf,
                           "%u  COMMIT  %x:r%d <- %x\n",
                           &time, pc, wbr, wbv);

                if (r != 4)
                        continue;

                *cycle = time / 10;
                return 1;
        }

}

void note_commit(unsigned pc, unsigned wbr, unsigned wbv)
{
        // if co-simulating, get the next commit event from the RTL
        // trace and compare

        if (enable_cosimulation) {
                unsigned rtl_cycle, rtl_pc, rtl_wbr, rtl_wbv;
                int r = get_rtl_commit(&rtl_cycle, &rtl_pc, &rtl_wbr, &rtl_wbv);
                n_cycle = rtl_cycle;

                if (!r) {
                        printf("RTL trace ended at this point\n");
                        exit(0);
                }

                if (rtl_pc != pc || rtl_wbr != wbr || rtl_wbv != wbv) {
                        printf("Divergence detected\n");

                        printf("ISA %08x: r%d <- %08x\n", pc, wbr, wbv);
                        printf("RTL %08x: r%d <- %08x\n", rtl_pc, rtl_wbr, rtl_wbv);

                        printf("RTL output leading up to this:\n");

                        unsigned i;
                        for (i = 0; i < KEEP_LINES; ++i) {
                                unsigned k = (i + last_p) % KEEP_LINES;
                                if (last[k])
                                        printf("%s", last[k]);
                        }

                        exit(1);
                }
        }
}

void print_coverage(void)
{
        int i;
        int n_tested = 0, n = 0;
        for (i = 0; i < 64+64+32; ++i)
                if (inst_name[i]) {
                        ++n;
                        if (coverage[i])
                                ++n_tested;
                }

        printf("\n"
               "Coverage: %d / %d (%4.2f%%)\n"
               "Untested instructions: ",
               n_tested, n,
               (100.0 * n_tested) / n);

        for (i = 0; i < 64+64+32; ++i)
                if (inst_name[i] && !coverage[i] /*< 20*/)
                        printf(" %s", inst_name[i]);
        putchar(' ');
}

void run_simple(MIPS_state_t *state)
{
        u_int32_t oldreg[32];
        u_int32_t pc_prev;
        u_int32_t pc_next = state->pc + 4;
        u_int32_t wbv, s = 0, t, st_old = 0;
        int wbr;

        int branch_delay_slot_next = 0;
        int branch_delay_slot = 0;
        int annul_delay_slot = 0;

        atexit(print_coverage);

        state->epc = 0xDEADBEEF;
        memset(state->r,    0, sizeof state->r);
        memset(oldreg,  0, sizeof oldreg);
        memset(state->cp0r, 0, sizeof state->cp0r);
        state->lo = state->hi = 0;

        // Status after reset
        state->cp0_status.ds_ts = 0;
        state->cp0_status.ds_sr = 0;
        state->cp0_status.erl   = 1;
        state->cp0_status.ds_bev= 1;

        for (;;) {
                int j;
                inst_t i;

                pc_prev = state->pc;
                if (!branch_delay_slot)
                        state->epc = state->pc;

                i.raw = annul_delay_slot ? 0 : load(state->pc, 4, 1);
                state->pc = pc_next;
                pc_next += sizeof(inst_t);

                if (enable_disass & !enable_regwrites)
                        for (j = 0; j < 32; ++j)
                                if (oldreg[j] != state->r[j])
                                        printf("\t\t\t\t\tr%d = 0x%08x\n",
                                               j, oldreg[j] = state->r[j]);
                branch_delay_slot = branch_delay_slot_next;
                branch_delay_slot_next = 0;
                annul_delay_slot = 0;

                s = state->r[i.r.rs];
                t = state->r[i.r.rt];
                wbr = i.r.rt;
                wbv = 0xDEADBEEF; // Never used, but have to shut up gcc.

                ++coverage[i.j.opcode == REG    ? 64 + i.r.funct :
                           i.j.opcode == REGIMM ? 128 + i.r.rt :
                           /*                  */ i.j.opcode];

                // Grab the old store value for comparison, but avoid IO
                st_old = 0; // IO accesses defaults to this
                if (enable_disass && ((s + i.i.imm) >> 24) != 0xFF)
                        switch (i.j.opcode) {
                        case SB:   st_old = LD8(s + i.i.imm); break;
                        case SH:   st_old = LD16(s + i.i.imm); break;
                        case SW:   st_old = LD32(s + i.i.imm); break;
                        }

                switch (i.j.opcode) {
                case REG: // all R-type, thus rd is target register
                        wbr = i.r.rd;
                        switch (i.r.funct) {
                        case SLL:  wbv = t << i.r.sa; break;
                        case SRL:  wbv = t >> i.r.sa; break;
                        case SRA:  wbv = (int)t >> i.r.sa; break;
                        case SLLV: wbv = t << (s & 31); break;
                        case SRLV: wbv = t >> (s & 31); break;
                        case SRAV: wbv = (int)t >> (s & 31); break;

                        case JALR: wbv = pc_next;
                        case JR:   pc_next = s;
                                   branch_delay_slot_next = 1;
                                   break;

                        case SYSCALL:
                                // XXX FATAL!
                                exit(0);
                                fatal("%08x:syscall(%d) not handled\n",
                                       pc_prev, state->r[2]);
                                break;

                        case MFHI: wbv = state->hi; break;
                        case MTHI: state->hi = s; break;
                        case MFLO: wbv = state->lo; break;
                        case MTLO: state->lo = s; break;
                        case MULT: {
                                int64_t i64 = (int64_t)s * (int64_t)t;
                                state->lo = i64;
                                state->hi = i64 >> 32;
                                break;
                        }
                        case MULTU: {
                                u_int64_t u64 = (u_int64_t)s * (u_int64_t)t;
                                state->lo = u64;
                                state->hi = u64 >> 32;
                                break;
                        }
                        case DIV:
                                if (t) {
                                        state->hi = (int)s % (int)t;
                                        state->lo = (int)s / (int)t;
                                } else {
                                        // XXX What is supposed to happen here?
                                }
                                break;
                        case DIVU:
                                if (t) {
                                        state->hi = s % t;
                                        state->lo = s / t;
                                } else {
                                        // XXX What is supposed to happen here?
                                }
                                break;
                        case ADD:  fprintf(stderr, "Instruction ADD not supported\n"), exit(1);
                                   wbv = s + t; break; // XXX Ignore the trap on overflow for now
                        case ADDU: wbv = s + t; break;
                        case SUB:  fprintf(stderr, "Instruction SUB not supported\n"), exit(1);
                                   wbv = s - t; break;
                        case SUBU: wbv = s - t; break;
                        case AND:  wbv = s & t; break;
                        case OR:   wbv = s | t; break;
                        case XOR:  wbv = s ^ t; break;
                        case NOR:  wbv = s |~ t; break;

                        case SLT:  wbv = (int) s < (int) t; break;
                        case SLTU: wbv = s < t; break;
                        case BREAK:
                                //pc_next = (cp0_status.bev ? 0xBFC00200 : 0x80000000) + 0x180;
                                pc_next = 0xBFC00380;
                                annul_delay_slot = 1;
                                state->cp0_status.exl = 1;
                                state->cp0_cause.exc_code = EXC_BP;
                                state->cp0_cause.bd = branch_delay_slot;
                                state->cp0r[CP0_EPC] = pc_prev;
                                break;
                        default:
                                fatal("REG sub-opcode %d not handled\n", i.r.funct);
                                break;
                        }
                        break;

                case REGIMM:  // All I-type
                        wbv = pc_next;
                        wbr = 0;
                        switch (i.r.rt) {
                        case BLTZAL: wbr = 31;
                        case BLTZ:
                                if ((int)s < 0)
                                        pc_next = state->pc + (i.i.imm << 2);
                                else
                                        wbr = 0;
                                branch_delay_slot_next = 1;
                                break;
                        case BGEZAL: wbr = 31;
                        case BGEZ:
                                if ((int)s >= 0)
                                        pc_next = state->pc + (i.i.imm << 2);
                                else
                                        wbr = 0;
                                branch_delay_slot_next = 1;
                                break;
                        default:
                                fatal("REGIMM rt=0d%d not handled\n", i.r.rt);
                        }
                        break;

                case JAL:
                        ++n_call;
                        wbr = 31; wbv = pc_next;
                        pc_next = (state->pc & ~((1<<28)-1)) | (i.j.offset << 2);
                        branch_delay_slot_next = 1;
                        break;
                case J: wbr = 0;
                        pc_next = (state->pc & ~((1<<28)-1)) | (i.j.offset << 2);
                        branch_delay_slot_next = 1;
                        break;

                case BEQ:
                        wbr = 0;
                        if (s == t)
                                pc_next = state->pc + (i.i.imm << 2);
                        branch_delay_slot_next = 1;
                        break;
                case BNE:
                        wbr = 0;
                        if (s != t)
                                pc_next = state->pc + (i.i.imm << 2);
                        branch_delay_slot_next = 1;
                        break;
                case BLEZ:
                        wbr = 0;
                        if (0 >= (int)s)
                                pc_next = state->pc + (i.i.imm << 2);
                        branch_delay_slot_next = 1;
                        break;
                case BGTZ:
                        wbr = 0;
                        if (0 < (int)s)
                                pc_next = state->pc + (i.i.imm << 2);
                        branch_delay_slot_next = 1;
                        break;

                case ADDI: wbv = (int) s + i.i.imm; break;
                case ADDIU:wbv =       s + i.i.imm; break;
                case SLTI: wbv = (int) s < (int) i.i.imm; break; // !!!
                case SLTIU:wbv = s < (unsigned) i.i.imm; break;
                case ANDI: wbv = s & i.u.imm; break;
                case ORI:  wbv = s | i.u.imm; break;
                case XORI: wbv = s ^ i.u.imm; break;
                case LUI:  wbv = i.u.imm << 16; break;
                case CP0:
                        /* Two possible formats */
                        if (i.r.rs & 0x10) {
                                if (i.r.funct == C0_ERET) {
                                        /* Exception Return */
                                        annul_delay_slot = 1;
                                        if (branch_delay_slot)
                                                fprintf(stderr, "ERET in a delay slot is illegal!\n");

                                        if (state->cp0_status.erl) {
                                                pc_next = state->cp0r[CP0_ERROREPC];
                                                state->cp0_status.erl = 0;
                                        } else {
                                                pc_next = state->cp0r[CP0_EPC];
                                                state->cp0_status.exl = 0;
                                        }
                                        break;
                                }
                                /* C1 format */
                                fprintf(stderr,
                                        "Unhandled CP0 command %s\n",
                                        i.r.funct == C0_TLBR  ? "tlbr" :
                                        i.r.funct == C0_TLBWI ? "tlbwi" :
                                        i.r.funct == C0_TLBWR ? "tlbwr" :
                                        i.r.funct == C0_TLBP  ? "tlbp" :
                                        i.r.funct == C0_ERET  ? "eret" :
                                        i.r.funct == C0_DERET ? "deret" :
                                        i.r.funct == C0_WAIT  ? "wait" :
                                        "???");
                        } else {
                                assert(i.r.funct == 0);
                                if (i.r.rs & 4) {
                                        wbr = 0;
                                        state->cp0r[i.r.rd] = t;
                                        switch (i.r.rd) {
                                        case CP0_INDEX:         //  0
                                                /*
                                                 * Five low-order bits
                                                 * to index an entry
                                                 * in the TLB.
                                                 */
                                                break;
                                        case CP0_RANDOM:        //  1
                                                /*
                                                 * Read-only! Index
                                                 * into the TBL
                                                 * incremented for
                                                 * every instruction
                                                 * executed.
                                                 */
                                                break;
                                        case CP0_ENTRYLO0:      //  2
                                        case CP0_ENTRYLO1:      //  3
                                                /*
                                                 * TBL Entry:
                                                 * 0:4 PFN:22 C:3 D:1 V:1 G:1
                                                 */
                                                break;
                                        case CP0_CONTEXT:       //  4
                                                /*
                                                 * Pointer to an entry
                                                 * in the page table
                                                 * entry array
                                                 * PTEBase:7 BadVPN2:21 0:4
                                                 */
                                                break;
                                        case CP0_PAGEMASK:      //  5
                                                break;
                                        case CP0_WIRED:         //  6
                                                /* Lower boundry of random TBL entry */
                                                break;
                                                //case CP0_INFO:                //  7
                                                //case CP0_BADVADDR:    //  8
                                                //case CP0_COUNT:               //  9
                                        case CP0_ENTRYHI:       // 10
                                                /*
                                                 * Holds the high-order bits of a TBL entry.
                                                 * VPN2:21 0:3 ASID:8
                                                 */
                                                break;
                                                //case CP0_COMPARE:     // 11
                                                //case CP0_STATUS:      // 12

                                        case CP0_STATUS:
                                                state->cp0_status.raw = t;
                                                state->cp0_status.res1 = state->cp0_status.res2 = 0;
                                                printf("Operating mode %s\n",
                                                       state->cp0_status.ksu == 0 ? "kernel" :
                                                       state->cp0_status.ksu == 1 ? "supervisor" :
                                                       state->cp0_status.ksu == 2 ? "user" : "??");
                                                printf("Exception level %d\n", state->cp0_status.exl);
                                                printf("Error level %d\n", state->cp0_status.erl);
                                                printf("Interrupts %sabled\n",
                                                       state->cp0_status.ie ? "en" : "dis");
                                                break;

                                                //case CP0_CAUSE:
                                        case CP0_EPC:
                                        case CP0_ERROREPC:
                                                break;

                                        default:
                                                fprintf(stderr, "Setting an unknown CP0 register %d\n", i.r.rd);
                                                // assert(0);
                                        }
                                } else {
                                        wbv = state->cp0r[i.r.rd];
                                }
                        }
                        break;
                case CP2: // 0x12 << 26 == 0x48000000
                        if (state->lo == 0x87654321) {
                                printf("TEST SUCCESS!\n");
                                exit(0);
                        } else {
                                printf("TEST FAILED WITH $2 = 0x%08x\n",
                                       state->lo);
                                exit(1);
                        }

                case LB:   wbv = EXT8(LD8(s + i.i.imm)); break;
                case LH:   wbv = EXT16(LD16(s + i.i.imm)); break;
                case LBU:  wbv = LD8(s + i.i.imm); break;
                case LHU:  wbv = LD16(s + i.i.imm); break;
                case LW:   wbv = LD32(s + i.i.imm); break;
                case SB:   wbr = 0; ST8(s + i.i.imm,t); break;
                case SH:   wbr = 0; ST16(s + i.i.imm,t); break;
                case SW:   wbr = 0; ST32(s + i.i.imm,t); break;
                case CP1:  {
                        if (i.r.rs == 16 /* S */)
                                fatal("%08x:%08x, opcode 0x%x.s not handled\n",
                                      pc_prev, i.raw, i.r.funct);
                        if (i.r.rs == 17 /* D */) {
                                if (i.r.funct == CP1_ADD)
                                        fatal("%08x:%08x, opcode add.d not handled\n",
                                              pc_prev, i.raw);
                                fatal("%08x:%08x, opcode 0x%x.d not handled\n",
                                      pc_prev, i.raw, i.r.funct);
                        }
                        fatal("%08x:%08x, opcode CP1 rs=0x%x not handled\n",
                              pc_prev, i.raw, i.r.rs);
                }
                case CP3:  fatal("%08x:%08x, opcode CP3 not handled\n", pc_prev, i.raw);
                case BEQL: fatal("%08x:%08x, opcode BEQL not handled\n", pc_prev, i.raw);
                case BNEL: fatal("%08x:%08x, opcode BEQL not handled\n", pc_prev, i.raw);
                case LWC1: // XXX how are we going to cosimulate this? Extend the wbr address space?
                           state->f[wbr] = LD32(s + i.i.imm);
                           wbr = 0;
                           break;

                case LDC1: fatal("%08x:%08x, opcode LDCP1 not handled\n", pc_prev, i.raw);
                case SWC1: fatal("%08x:%08x, opcode SWCP1 not handled\n", pc_prev, i.raw);
                case SDC1: fatal("%08x:%08x, opcode SDCP1 not handled\n", pc_prev, i.raw);
                default:   fatal("%08x:%08x, opcode %d not handled\n",
                                 pc_prev, i.raw, i.j.opcode);
                           break; // XXX
                }

                // Statistics
                ++n_issue;

                if (wbr) {
                        state->r[wbr] = wbv;
                }

                if (enable_disass_user && (pc_prev & 0xF0000000) == 0x40000000)
                        enable_disass = 1;

                if (enable_disass) {
                        printf("%08x %08x ", pc_prev, i.raw);
                        disass(pc_prev,i);
                        if (enable_regwrites && wbr)
                                printf("$%d <- %08x", wbr, wbv);
                        switch (i.j.opcode) {
                        case LB:
                        case LH:
                        case LBU:
                        case LHU:
                        case LW:   printf(" [%08x]", s + i.i.imm); break;
                        case SB:   printf("[%08x] <- %02x (was %02x)",
                                          s + i.i.imm, t & 0xFF, st_old);
                                   break;
                        case SH:   printf("[%08x] <- %04x (was %04x)",
                                          s + i.i.imm, t & 0xFFFF, st_old);
                                   break;
                        case SW:   printf("[%08x] <- %08x (was %08x)",
                                          s + i.i.imm, t, st_old);
                                   break;
                        default:
                                if (!enable_regwrites || !wbr)
                                        printf(".");
                        }
                        printf("\n");
                }

                if (wbr)
                        note_commit(pc_prev, wbr, wbv);
        }
// END:;

}

// Local Variables:
// mode: C
// c-style-variables-are-local-p: t
// c-file-style: "linux"
// End:
