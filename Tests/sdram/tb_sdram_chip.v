// Testbench of the SDRAM chip model, sdram_chip. See tb_unit.vh for how the vectors are
// applied: each vector is one cycle of the chip's pins, which change at the falling edge
// before the chip's rising edge. The pause after power up is 100 ns here.
//
// The outputs of each vector, in hex, with x and z as Icarus writes them:
//   dq       DQ at the rising edge, where a controller captures it
//   between  DQ 4 ns after the rising edge, between tOH (3 ns) and tAC (6 ns)
//   error    the model's error number, 0 while every rule holds
`timescale 1ns / 1ps
module tb_sdram_chip;

`include "tb_unit.vh"

reg        cke;
reg  [3:0] command;  // {CS#, RAS#, CAS#, WE#}
reg  [1:0] ba;
reg  [12:0] a;
reg  [1:0] dqm;
reg        dq_oe;
reg  [15:0] dq_out;
wire [15:0] dq;
wire [4:0] error;

reg  [15:0] dq_edge;
reg  [15:0] dq_between;

assign dq = dq_oe ? dq_out : 16'bz;

sdram_chip #(
    .PAUSE(100_000)
) dut (
    .clk(clk),
    .cke(cke),
    .cs_n(command[3]),
    .ras_n(command[2]),
    .cas_n(command[1]),
    .we_n(command[0]),
    .ba(ba),
    .a(a),
    .dqm(dqm),
    .dq(dq),
    .error(error)
);

always @(posedge clk) begin
    dq_edge <= dq;
    #4 dq_between = dq;
end

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h\n", label, cke, command, ba, a,
                   dqm, dq_oe, dq_out) == 8)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %0d\n", dq_edge, dq_between, error);
    tb_close;
end

endmodule
