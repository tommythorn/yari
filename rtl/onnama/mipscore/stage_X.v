// -----------------------------------------------------------------------
//
//   Copyright 2004 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------

`timescale 1ns/10ps
`include "mips_asm.v"

module stage_X(input  wire        clk
              ,input  wire        stall
              ,input  wire        flush

              ,input  wire        d_valid
              ,input  wire [31:0] d_instr
              ,input  wire [31:0] d_pc
              ,input  wire [31:0] d_npc
              ,input  wire [ 5:0] d_opcode
              ,input  wire [ 5:0] d_fn
              ,input  wire [ 4:0] d_rd
              ,input  wire [ 5:0] d_rs
              ,input  wire [ 5:0] d_rt
              ,input  wire [ 4:0] d_sa
              ,input  wire [31:0] d_op1_val
              ,input  wire [31:0] d_op2_val
              ,input  wire [31:0] d_rt_val
              ,input  wire [ 5:0] d_wbr
              ,input  wire [31:0] d_target

              ,output reg  [31:0] x_pc            = 0
              ,output reg         x_valid         = 0
              ,output reg  [32:0] x_instr         = 0   // XXX for debugging only
              ,output reg  [ 6:0] x_opcode        = 0
              ,output reg  [31:0] x_op1_val       = 0 // XXX
              ,output reg  [31:0] x_rt_val        = 0 // for stores only
              ,output reg  [ 5:0] x_wbr           = 0
              ,output reg  [31:0] x_res           = 0

              ,output reg         x_jump          = 0
              ,output reg         x_annul_delay_slot = 0
              ,output reg  [31:0] x_target        = 0
              ,output wire        x_hazzard
              ,output wire [ 7:0] debug_byte
              );

   parameter debug = 0;

   wire               ops_eq = d_op1_val == d_op2_val;
   reg [31:0]         dummy = 0;
   wire [32:0]        subtracted = {d_op1_val[31],d_op1_val} - {d_op2_val[31],d_op2_val};

   wire [4:0]         shift_dist = d_fn[2] ? d_op1_val[4:0] : d_sa;
   wire [31:0]        ashift_out;
   wire [31:0]        lshift_out;

   reg [31:0]         x_lo = 0, x_hi = 0;

   reg [31:0]         cp0_status = 0,
                      cp0_epc = 0,
                      cp0_errorepc = 0,
                      cp0_cause = 0;

   assign             debug_byte = x_lo[7:0];

   arithshiftbidir arithshiftbidir(
        .distance(shift_dist),
        .data(d_op2_val),
        .direction(d_fn[1]),
        .result(ashift_out));

   logshiftright logshiftright(
        .distance(shift_dist),
        .data(d_op2_val),
        .result(lshift_out));

   /*
   wire               d_mult_strobe =
                      d_opcode == `REG && (d_fn == `MULT || d_fn == `MULTF);

   mult mult(.clk(clk),

             .op1 (d_op1_val),
             .op2 (d_op2_val),
             .go  (d_mult_strobe),

             .hold(x_mult_hold),
             .res (x_mult_res));
    */

   // Simulate FU busy conditions
   reg [32:0] lfsr = 0;
   always @(posedge clk)
     lfsr <= {lfsr[31:0], ~lfsr[32] ^ lfsr[19]};
   assign x_hazzard = 0 & ~lfsr[31];

   always @(posedge clk)
      if (flush) begin
`ifdef SIMULATE_MAIN
         if (debug)
           $display("%5dc EX: flushed", $time);
`endif
         x_instr   <= 0;
         x_pc      <= 0;
         x_valid   <= 0;
         x_opcode  <= 0;
         x_op1_val <= 0;
         x_rt_val  <= 0;
         x_wbr     <= 0;
         x_target  <= 0;
         x_res     <= 0;
         x_jump    <= 0;
         x_annul_delay_slot <= 0;
      end else if (~stall) begin
        x_instr   <= {~d_valid,d_instr};
        x_pc      <= d_pc;
        x_valid   <= d_valid;
        x_opcode  <= {~d_valid,d_opcode};
        x_op1_val <= d_op1_val;
        x_rt_val  <= d_rt_val;
        x_wbr     <= d_wbr;
        x_target  <= d_target;

        x_res     <= 'hDEADBEEF;
        x_jump    <= 0;
        x_annul_delay_slot <= 0;

`ifdef SIMULATE_MAIN
        // XXX Debug
        if (d_opcode != d_instr[31:26])
            $display("%5dc EX: d_opcode %x != d_instr[31:26] %x", $time,
                     d_opcode, d_instr[31:26]);
        if (d_fn != d_instr[5:0])
            $display("%5dc EX: d_fn %x != d_instr[5:0] %x", $time,
                     d_fn, d_instr[5:0]);
`endif

      case ({~d_valid,d_opcode})
        `REG:
          case (d_fn)
            `SLL: x_res <= ashift_out;
            `SRL: x_res <= lshift_out;
            `SRA: x_res <= ashift_out;

            `SLLV: x_res <= ashift_out;
            `SRLV: x_res <= lshift_out;
            `SRAV: x_res <= ashift_out;

            `JALR:
              begin
                 x_res    <= d_npc + 4;
                 x_target <= d_op1_val;
                 x_jump   <= 1;
              end
            `JR:
              begin
                 x_target <= d_op1_val;
                 x_jump   <= 1;
                 //$display("%5dc EX: JR %8x", $time, d_op1_val);
              end

            `MFHI: begin x_res <= x_hi; end
            `MFLO: begin x_res <= x_lo; end
            `MTHI: begin x_hi <= d_op1_val; end
            `MTLO: begin x_lo <= d_op1_val; end

            //`MULTU: reg_mul.u64 = (u_int64_t)s * (u_int64_t)t;
            //`DIV:   reg_mul.u32[1] = (int)s % (int)t;
            //        reg_mul.u32[0] = (int)s / (int)t; break;
            //`DIVU:  reg_mul.u32[1] = s % t;
            //        reg_mul.u32[0] = s / t; break;

/*
            `MULT: begin
                   {x_hi,x_lo} <= d_op1_val * d_op2_val;
               $display("%05dc EX: %d * %d", $time, d_op1_val, d_op2_val);
               end
*/
            `ADD:    x_res <= d_op1_val + d_op2_val;// XXX Trap on overflow later
            `ADDU:   x_res <= d_op1_val + d_op2_val;
            `SUB:    x_res <= d_op1_val - d_op2_val;// XXX Trap on overflow later
            `SUBU:   x_res <= d_op1_val - d_op2_val;
            `AND:    x_res <= d_op1_val & d_op2_val;
            `OR:     x_res <= d_op1_val | d_op2_val;
            `XOR:    x_res <= d_op1_val ^ d_op2_val;
            `NOR:    x_res <= d_op1_val | ~d_op2_val;

            `SLT:    x_res <= {{31{1'b0}}, subtracted[32]};
            `SLTU: if (d_op1_val < d_op2_val) x_res <= 1; else x_res <= 0;
            `BREAK: begin
               x_jump <= 1;
               x_annul_delay_slot <= 1;
               x_target <= 'hBFC00380;
               cp0_status[`CP0_STATUS_EXL] <= 1;
               //cp0_cause.exc_code = EXC_BP;
               cp0_cause <= 9 << 2;
               // cp0_cause.bd = branch_delay_slot; // XXX DELAY SLOT HANDLING!
               cp0_epc <= d_pc; // XXX DELAY SLOT HANDLING!
            end

`ifdef SIMULATE_MAIN
            default:
              $display("%05dc EX: %8x:%8x unhandled REG function %x", $time,
                       d_pc, d_instr, d_fn);
`endif
          endcase
        `REGIMM: // BLTZ, BGEZ, BLTZAL, BGEZAL
                 begin x_jump <= d_rt[0] ^ d_op1_val[31]; x_res  <= d_npc + 4; end
        `JAL:    begin x_jump <= 1;                       x_res  <= d_npc + 4; end
        `J:    x_jump <= 1;
        `BEQ:  x_jump <= ops_eq;
        `BNE:  begin x_jump <= ~ops_eq;
/*$display("%05d BNE %d target %8x", $time, ~ops_eq, d_target);*/ end

        `BLEZ: x_jump <= d_op1_val[31] || d_op1_val == 0;
        `BGTZ: x_jump <= !d_op1_val[31] && d_op1_val != 0; // XXX Share logic

        `ADDI: x_res <= d_op1_val + d_op2_val; // XXX OVERFLOW CHECK!
        `ADDIU:x_res <= d_op1_val + d_op2_val;
        `SLTI: begin x_res <= 0; {x_res[0], dummy} <= d_op1_val - d_op2_val; end
        // `SLTI:  x_res <= (int) d_op1_val < i.i.imm; // XXX ???
        //         res = d_op1_val - d_op2_val (33 bit arith to avoid
        //               worrying about overflow)
        //         x_jump = res < 0
        //

        `SLTIU:x_res <= d_op1_val < d_op2_val;  //s < i.i.imm;
        `ANDI: x_res <= {16'b0, d_op1_val[15:0] & d_op2_val[15:0]};
        `ORI:  x_res <= {d_op1_val[31:16], d_op1_val[15:0] | d_op2_val[15:0]};
        `XORI: x_res <= {d_op1_val[31:16], d_op1_val[15:0] ^ d_op2_val[15:0]};
        `LUI:  x_res <= {d_op2_val[15:0], {16{1'b0}}};

        // `CP1:
`ifdef SIMULATE_MAIN
        `CP2: begin
                #2
                if (x_lo == 32'h87654321)
                   $display("TEST SUCCEEDED!");
                else
                   $display("TEST FAILED WITH %x", x_lo);
                $finish; // XXX do something more interesting for real hw.
              end
`endif
        // `BBQL:

        `LB:   x_res <= d_op1_val + d_op2_val;
        `LBU:  x_res <= d_op1_val + d_op2_val;
        `LH:   x_res <= d_op1_val + d_op2_val;
        `LHU:  x_res <= d_op1_val + d_op2_val;
        `LW:   x_res <= d_op1_val + d_op2_val;
        `SB:   x_res <= d_op1_val + d_op2_val;
        `SH:   x_res <= d_op1_val + d_op2_val;
        `SW:   x_res <= d_op1_val + d_op2_val;

        `CP0: begin
          /* Two possible formats */
          if (d_rs[4]) begin
             if (d_fn == `C0_ERET) begin
                /* Exception Return */
                x_jump   <= 1;
                x_annul_delay_slot <= 1;
                if (cp0_status[`CP0_STATUS_ERL]) begin
                   x_target <= cp0_errorepc;
                   cp0_status[`CP0_STATUS_ERL] <= 0;
                   $display("ERET ERROREPC %x", cp0_errorepc);
                end else begin
                   x_target <= cp0_epc;
                   cp0_status[`CP0_STATUS_EXL] <= 0;
                   $display("ERET EPC %x", cp0_epc);
                end
             end
`ifdef SIMULATE_MAIN
             else
                /* C1 format */
                $display("Unhandled CP0 command %s\n",
                         d_fn == `C0_TLBR  ? "tlbr" :
                         d_fn == `C0_TLBWI ? "tlbwi" :
                         d_fn == `C0_TLBWR ? "tlbwr" :
                         d_fn == `C0_TLBP  ? "tlbp" :
                         d_fn == `C0_ERET  ? "eret" :
                         d_fn == `C0_DERET ? "deret" :
                         d_fn == `C0_WAIT  ? "wait" :
                         "???");
`endif
          end else begin
             if (d_rs[2])
               $display("MTCP0 r%d <- %x", d_rd, d_op2_val);
             else
               $display("MFCP0 r%d", d_rd);

`ifdef SIMULATE_MAIN
             if (d_fn != 0) $display("d_fn == %x", d_fn);
`endif
             if (d_rs[2]) begin
                x_wbr <= 0; // XXX
                // cp0regs[i.r.rd] = t;
                case (d_rd)
                  `CP0_STATUS:   begin
                                 cp0_status   <= d_op2_val;
                                 $display("STATUS <= %x", d_op2_val);
                                 end
                  `CP0_CAUSE:    begin
                                 cp0_cause <= d_op2_val;
                                 $display("CAUSE <= %x", d_op2_val);
                                 end
                  `CP0_EPC:      begin
                                 cp0_epc      <= d_op2_val;
                                 $display("EPC <= %x", d_op2_val);
                                 end
                  `CP0_ERROREPC: begin
                                 cp0_errorepc <= d_op2_val;
                                 $display("ERROREPC <= %x", d_op2_val);
                                 end
/*
                        cp0_status.raw = t;
                        cp0_status.res1 = cp0_status.res2 = 0;
                        printf("Operating mode %s\n",
                               cp0_status.ksu == 0 ? "kernel" :
                               cp0_status.ksu == 1 ? "supervisor" :
                               cp0_status.ksu == 2 ? "user" : "??");
                        printf("Exception level %d\n", cp0_status.exl);
                        printf("Error level %d\n", cp0_status.erl);
                        printf("Interrupts %sabled\n", cp0_status.ie ? "en" : "dis");
                        break;
*/
`ifdef SIMULATE_MAIN
                  default:
                    $display("Setting an unknown CP0 register %d", d_rd);
`endif
                        //case CP0_CAUSE:
                endcase
             end else
               case (d_rd)
                 `CP0_STATUS:   x_res <= cp0_status;   // 12
                 `CP0_CAUSE:    x_res <= cp0_cause;    // 13
                 `CP0_EPC:      x_res <= cp0_epc;      // 14
                 `CP0_ERROREPC: x_res <= cp0_errorepc; // 30
`ifdef SIMULATE_MAIN
                  default:
                    $display("Accessing an unknown CP0 register %d", d_rd);
`endif
               endcase
          end
`ifdef NOT_DONE
                  `CP0_INDEX:         //  0
                    /*
                     * Five low-order bits
                     * to index an entry
                     * in the TLB.
                     */
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

                                                break;

                                        default:
                                                fprintf(stderr, "Setting an unknown CP0 register %d\n", i.r.rd);
                                                // assert(0);
                                        }
                                } else {
                                        wbv = cp0regs[i.r.rd];
                                }
`endif
        end
`ifdef SIMULATE_MAIN
        default:
          if (d_valid)
            $display("%05dc EX: %8x:%8x unhandled opcode %x", $time,
                     d_pc, d_instr, d_opcode);
`endif
      endcase
`ifdef SIMULATE_MAIN
      if (debug)
        $display("%05dc EX: op1 %x op2 %x", $time, d_op1_val, d_op2_val);
`endif
   end

`ifdef SIMULATE_MAIN
initial if (debug && 0) $monitor(
"%05dv EX [MON]: stall %x d_valid %x d_instr %x d_pc %x d_npc %x d_opcode %x d_fn %x d_rt %x d_sa %x d_op1_val %x d_op2_val %x d_rt_val %x d_wbr %x d_target %x",
$time,
stall,d_valid,d_instr,d_pc,d_npc,d_opcode,d_fn,d_rt,d_sa,d_op1_val,d_op2_val,d_rt_val,d_wbr,d_target);
`endif
endmodule
