// Testbench of cpu_branch. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_branch;

`include "tb_unit.vh"

reg  [31:0] a;
reg  [31:0] b;
reg  [2:0]  branch_op;
reg         branch_signed;
wire        taken;

cpu_branch dut (
    .a(a),
    .b(b),
    .branch_op(branch_op),
    .branch_signed(branch_signed),
    .taken(taken)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h\n", label, a, b, branch_op, branch_signed) == 5)
        @(negedge clk) $fwrite(outputs_fd, "%h\n", taken);
    tb_close;
end

endmodule
