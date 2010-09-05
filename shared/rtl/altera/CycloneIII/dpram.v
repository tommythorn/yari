// synopsys translate_off
`timescale 1 ps / 1 ps
// synopsys translate_on
module dpram (clock,
              address_a, byteena_a, wrdata_a, wren_a, rddata_a,
              address_b, byteena_b, wrdata_b, wren_b, rddata_b);

        parameter DATA_WIDTH = 32;
        parameter ADDR_WIDTH =  7;
        parameter INIT_FILE  = "somefile"; // No .mif!

        input   [ADDR_WIDTH-1:0]      address_a;
        input   [ADDR_WIDTH-1:0]      address_b;
        input   [DATA_WIDTH/8-1:0]  byteena_a;
        input   [DATA_WIDTH/8-1:0]  byteena_b;
        input                         clock;
        input   [DATA_WIDTH-1:0]    wrdata_a;
        input   [DATA_WIDTH-1:0]    wrdata_b;
        input                         wren_a;
        input                         wren_b;
        output  [DATA_WIDTH-1:0]    rddata_a;
        output  [DATA_WIDTH-1:0]    rddata_b;

        wire [DATA_WIDTH-1:0] sub_wire0;
        wire [DATA_WIDTH-1:0] sub_wire1;
        wire [DATA_WIDTH-1:0] rddata_a = sub_wire0[DATA_WIDTH-1:0];
        wire [DATA_WIDTH-1:0] rddata_b = sub_wire1[DATA_WIDTH-1:0];

        altsyncram      altsyncram_component (
                                .wren_a (wren_a),
                                .clock0 (clock),
                                .wren_b (wren_b),
                                .byteena_a (byteena_a),
                                .byteena_b (byteena_b),
                                .address_a (address_a),
                                .address_b (address_b),
                                .data_a (wrdata_a),
                                .data_b (wrdata_b),
                                .q_a (sub_wire0),
                                .q_b (sub_wire1),
                                .aclr0 (1'b0),
                                .aclr1 (1'b0),
                                .addressstall_a (1'b0),
                                .addressstall_b (1'b0),
                                .clock1 (1'b1),
                                .clocken0 (1'b1),
                                .clocken1 (1'b1),
                                .clocken2 (1'b1),
                                .clocken3 (1'b1),
                                .eccstatus (),
                                .rden_a (1'b1),
                                .rden_b (1'b1));
        defparam
                altsyncram_component.address_aclr_a = "NONE",
                altsyncram_component.address_aclr_b = "NONE",
                altsyncram_component.address_reg_b = "CLOCK0",
                altsyncram_component.byteena_aclr_a = "NONE",
                altsyncram_component.byteena_aclr_b = "NONE",
                altsyncram_component.byteena_reg_b = "CLOCK0",
                altsyncram_component.byte_size = 8,
                altsyncram_component.indata_aclr_a = "NONE",
                altsyncram_component.indata_aclr_b = "NONE",
                altsyncram_component.indata_reg_b = "CLOCK0",
                altsyncram_component.init_file = {INIT_FILE,".mif"},
                altsyncram_component.intended_device_family = "Cyclone III",
                altsyncram_component.lpm_type = "altsyncram",
                altsyncram_component.numwords_a = 1 << ADDR_WIDTH,
                altsyncram_component.numwords_b = 1 << ADDR_WIDTH,
                altsyncram_component.operation_mode = "BIDIR_DUAL_PORT",
                altsyncram_component.outdata_aclr_a = "NONE",
                altsyncram_component.outdata_aclr_b = "NONE",
                altsyncram_component.outdata_reg_a = "UNREGISTERED",
                altsyncram_component.outdata_reg_b = "UNREGISTERED",
                altsyncram_component.power_up_uninitialized = "FALSE",
                altsyncram_component.ram_block_type = "M9K",
                altsyncram_component.read_during_write_mode_mixed_ports = "DONT_CARE", // "OLD_DATA",
                altsyncram_component.widthad_a = ADDR_WIDTH,
                altsyncram_component.widthad_b = ADDR_WIDTH,
                altsyncram_component.width_a = DATA_WIDTH,
                altsyncram_component.width_b = DATA_WIDTH,
                altsyncram_component.width_byteena_a = DATA_WIDTH/8,
                altsyncram_component.width_byteena_b = DATA_WIDTH/8,
                altsyncram_component.wrcontrol_aclr_a = "NONE",
                altsyncram_component.wrcontrol_aclr_b = "NONE",
                altsyncram_component.wrcontrol_wraddress_reg_b = "CLOCK0";
endmodule
