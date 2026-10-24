// Testbench of cpu_hazard. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_hazard;

`include "tb_unit.vh"

reg  busy_if;
reg  busy_id;
reg  busy_ex;
reg  busy_mem;
reg  redirect;
wire stall_if;
wire stall_id;
wire flush_id;
wire stall_ex;
wire flush_ex;
wire stall_mem;
wire flush_mem;
wire flush_wb;

cpu_hazard dut (
    .busy_if(busy_if),
    .busy_id(busy_id),
    .busy_ex(busy_ex),
    .busy_mem(busy_mem),
    .redirect(redirect),
    .stall_if(stall_if),
    .stall_id(stall_id),
    .flush_id(flush_id),
    .stall_ex(stall_ex),
    .flush_ex(flush_ex),
    .stall_mem(stall_mem),
    .flush_mem(flush_mem),
    .flush_wb(flush_wb)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h\n",
                   label, busy_if, busy_id, busy_ex, busy_mem, redirect) == 6)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h %h %h %h\n", stall_if, stall_id,
                               flush_id, stall_ex, flush_ex, stall_mem, flush_mem, flush_wb);
    tb_close;
end

endmodule
