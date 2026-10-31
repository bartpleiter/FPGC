// Testbench of cpu_interrupts. See tb_unit.vh for how the vectors are applied.
//
// The outputs are written at the rising edge, just before it clocks the take, because
// that's what the instruction in MEM sees. Three reset cycles with all inputs at 0 come
// before the vectors, so the synchronizer starts from a known state. The test can set the
// number of inputs with the parameter.
`timescale 1ns / 1ps
module tb_cpu_interrupts #(
    parameter INTERRUPTS = 7
);

`include "tb_unit.vh"

reg                 reset;
reg  [INTERRUPTS:1] interrupts;
reg                 take;
wire                pending;
wire [7:0]          id;

cpu_interrupts #(
    .INTERRUPTS(INTERRUPTS)
) dut (
    .clk(clk),
    .reset(reset),
    .interrupts(interrupts),
    .take(take),
    .pending(pending),
    .id(id)
);

initial begin
    tb_open;
    reset      = 1'b1;
    interrupts = {INTERRUPTS{1'b0}};
    take       = 1'b0;
    repeat (3) @(negedge clk);
    while ($fscanf(vectors_fd, "%h %h %h %h\n", label, reset, interrupts, take) == 4) begin
        @(posedge clk) $fwrite(outputs_fd, "%h %h\n", pending, id);
        @(negedge clk);
    end
    tb_close;
end

endmodule
