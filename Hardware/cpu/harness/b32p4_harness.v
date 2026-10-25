// Timing harness of the CPU, the top level of `fpgc cpu timing` without files. It isn't
// CPU RTL. It puts b32p4 between the memories it reaches, at their real sizes, and keeps
// its outputs alive with the sink.
//
// For now that's the ROM, as the fetch memory, and one data memory of the size of the
// future L1D data array (4 KiB, in four byte lanes). Every result can end up in a
// write, so the data port's outputs going to the sink keeps the whole datapath.
module b32p4_harness (
    input  wire clk,
    input  wire din,
    output wire dout
);

wire [7:0] random;
source #(8) src (.clk(clk), .din(din), .q(random));

wire reset = random[0];

// The ROM, 4 KiB, filled with arbitrary words so Quartus can't simplify the CPU
reg [31:0] rom [0:1023];
integer k;
initial for (k = 0; k < 1024; k = k + 1) rom[k] = k * 32'h9E3779B9;

wire [31:0] fetch_addr;
wire        fetch_en;
reg  [31:0] fetch_word;

always @(posedge clk)
    if (fetch_en)
        fetch_word <= rom[fetch_addr[11:2]];

wire [31:0] data_addr;
wire [31:0] data_write_data;
wire [3:0]  data_byte_enable;
wire        sdram_write;
wire [31:0] sdram_word;

// The data memory, one M9K per byte lane
genvar lane;
generate
    for (lane = 0; lane < 4; lane = lane + 1) begin : data_lane
        reg [7:0] ram [0:1023];
        reg [7:0] q;
        always @(posedge clk) begin
            if (sdram_write && data_byte_enable[lane])
                ram[data_addr[11:2]] <= data_write_data[8*lane +: 8];
            q <= ram[data_addr[11:2]];
        end
        assign sdram_word[8*lane +: 8] = q;
    end
endgenerate

/* verilator lint_off PINCONNECTEMPTY */
b32p4 cpu (
    .clk(clk),
    .reset(reset),
    .fetch_addr(fetch_addr),
    .fetch_en(fetch_en),
    .fetch_word(fetch_word),
    .data_addr(data_addr),
    .data_write_data(data_write_data),
    .data_byte_enable(data_byte_enable),
    .sdram_write(sdram_write),
    .sdram_word(sdram_word),
    .retire_valid(),
    .retire_pc(),
    .retire_instr(),
    .retire_writes_dreg(),
    .retire_dreg(),
    .retire_dreg_value(),
    .retire_write(),
    .retire_write_addr(),
    .retire_write_data(),
    .retire_write_be(),
    .retire_push(),
    .retire_push_value(),
    .retire_pop(),
    .retire_pop_value(),
    .retire_redirect(),
    .retire_target(),
    .retire_irq(),
    .retire_irq_id()
);
/* verilator lint_on PINCONNECTEMPTY */

sink #(69) out (
    .clk(clk),
    .d({data_addr, data_write_data, data_byte_enable, sdram_write}),
    .dout(dout)
);

endmodule
