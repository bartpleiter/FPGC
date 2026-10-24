// Timing harness of the CPU, the top level of `fpgc cpu timing` without files. It isn't
// CPU RTL. It puts b32p4 between the memories it reaches, at their real sizes, and keeps
// its outputs alive with the sink.
//
// For now that's only the ROM, as the fetch memory. Until the CPU has a data port, the
// register write of the retire port is the only way out for the results, so it goes
// to the sink too. Without it, Quartus would remove the whole datapath.
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

wire        retire_writes_dreg;
wire [3:0]  retire_dreg;
wire [31:0] retire_dreg_value;

/* verilator lint_off PINCONNECTEMPTY */
b32p4 cpu (
    .clk(clk),
    .reset(reset),
    .fetch_addr(fetch_addr),
    .fetch_en(fetch_en),
    .fetch_word(fetch_word),
    .retire_valid(),
    .retire_pc(),
    .retire_instr(),
    .retire_writes_dreg(retire_writes_dreg),
    .retire_dreg(retire_dreg),
    .retire_dreg_value(retire_dreg_value),
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

sink #(37) out (
    .clk(clk),
    .d({retire_writes_dreg, retire_dreg, retire_dreg_value}),
    .dout(dout)
);

endmodule
