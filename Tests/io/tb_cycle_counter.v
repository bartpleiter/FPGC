// Testbench of cycle_counter. See tb_unit.vh for how the vectors are applied.
// A vector with `poke` set first puts `poke_count` into the counter, so the tests can
// reach the high word without running for 2^32 cycles.
`timescale 1ns / 1ps
module tb_cycle_counter;

`include "tb_unit.vh"

reg         reset;
reg         take_snapshot;
reg         poke;
reg  [63:0] poke_count;
wire [63:0] snapshot;

cycle_counter dut (
    .clk(clk),
    .reset(reset),
    .take_snapshot(take_snapshot),
    .snapshot(snapshot)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h\n", label, reset, take_snapshot, poke,
                   poke_count) == 5) begin
        if (poke)
            dut.count = poke_count;
        @(negedge clk) $fwrite(outputs_fd, "%h %h\n", snapshot, dut.count);
    end
    tb_close;
end

endmodule
