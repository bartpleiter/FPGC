// Testbench of cpu_forward. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_forward;

`include "tb_unit.vh"

reg  [3:0] areg;
reg  [3:0] breg;
reg        reads_areg;
reg        reads_breg;
reg        ex_writes_dreg;
reg  [3:0] ex_dreg;
reg        ex_late;
reg        mem_writes_dreg;
reg  [3:0] mem_dreg;
wire       areg_from_mem;
wire       areg_from_wb;
wire       breg_from_mem;
wire       breg_from_wb;
wire       late_result_stall;

cpu_forward dut (
    .areg(areg),
    .breg(breg),
    .reads_areg(reads_areg),
    .reads_breg(reads_breg),
    .ex_writes_dreg(ex_writes_dreg),
    .ex_dreg(ex_dreg),
    .ex_late(ex_late),
    .mem_writes_dreg(mem_writes_dreg),
    .mem_dreg(mem_dreg),
    .areg_from_mem(areg_from_mem),
    .areg_from_wb(areg_from_wb),
    .breg_from_mem(breg_from_mem),
    .breg_from_wb(breg_from_wb),
    .late_result_stall(late_result_stall)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h %h %h\n", label, areg, breg,
                   reads_areg, reads_breg, ex_writes_dreg, ex_dreg, ex_late,
                   mem_writes_dreg, mem_dreg) == 10)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h\n", areg_from_mem, areg_from_wb,
                               breg_from_mem, breg_from_wb, late_result_stall);
    tb_close;
end

endmodule
