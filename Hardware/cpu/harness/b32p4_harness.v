// Timing harness of the CPU, used for timing tests in Quartus without having the full FPGC ready yet.
module b32p4_harness #(
    parameter L1I_LINES = 128,
    parameter L1D_LINES = 512
) (
    input  wire clk,
    input  wire din,
    output wire dout
);

wire [42:0] random;
source #(43) src (.clk(clk), .din(din), .q(random));

wire        reset            = random[0];
wire [7:1]  interrupts       = random[7:1];
wire        sdram_take       = random[8];
wire        sdram_read_valid = random[9];
wire        sdram_next       = random[10];
wire [31:0] sdram_read_word  = random[42:11];

// The ROM, 4 KiB, filled with arbitrary words so Quartus can't simplify the CPU
reg [31:0] rom [0:1023];
integer k;
initial for (k = 0; k < 1024; k = k + 1) rom[k] = k * 32'h9E3779B9;

wire [31:0] fetch_addr;
wire        fetch_en;
reg  [31:0] rom_word;

always @(posedge clk)
    if (fetch_en)
        rom_word <= rom[fetch_addr[11:2]];

wire        sdram_req;
wire        sdram_write;
wire [25:5] sdram_line;
wire [2:0]  sdram_first;
wire [31:0] sdram_write_word;

/* verilator lint_off PINCONNECTEMPTY */
b32p4 #(
    .L1I_LINES(L1I_LINES),
    .L1D_LINES(L1D_LINES)
) cpu (
    .clk(clk),
    .reset(reset),
    .fetch_addr(fetch_addr),
    .fetch_en(fetch_en),
    .rom_word(rom_word),
    .sdram_req(sdram_req),
    .sdram_write(sdram_write),
    .sdram_line(sdram_line),
    .sdram_first(sdram_first),
    .sdram_take(sdram_take),
    .sdram_read_valid(sdram_read_valid),
    .sdram_read_word(sdram_read_word),
    .sdram_next(sdram_next),
    .sdram_write_word(sdram_write_word),
    .interrupts(interrupts),
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

sink #(58) out (
    .clk(clk),
    .d({sdram_req, sdram_write, sdram_line, sdram_first, sdram_write_word}),
    .dout(dout)
);

endmodule
