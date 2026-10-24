// Testbench of cpu_regfile. See tb_unit.vh for how the vectors are applied.
//
// The outputs are written at the rising edge, just before it
// clocks the write, because that's when ID/EX takes them in the pipeline. So reading the
// register that is being written shows the write-through.
`timescale 1ns / 1ps
module tb_cpu_regfile;

`include "tb_unit.vh"

reg         reset;
reg  [3:0]  areg;
reg  [3:0]  breg;
reg         writes_dreg;
reg  [3:0]  dreg;
reg  [31:0] dreg_value;
wire [31:0] areg_value;
wire [31:0] breg_value;

cpu_regfile dut (
    .clk(clk),
    .reset(reset),
    .areg(areg),
    .breg(breg),
    .areg_value(areg_value),
    .breg_value(breg_value),
    .writes_dreg(writes_dreg),
    .dreg(dreg),
    .dreg_value(dreg_value)
);

// The registers as wires for the waveform, because Icarus doesn't dump arrays.
wire [31:0] r1 = dut.regs[1], r2 = dut.regs[2], r3 = dut.regs[3], r4 = dut.regs[4], r5 = dut.regs[5],
            r6 = dut.regs[6], r7 = dut.regs[7], r8 = dut.regs[8], r9 = dut.regs[9], r10 = dut.regs[10],
            r11 = dut.regs[11], r12 = dut.regs[12], r13 = dut.regs[13], r14 = dut.regs[14], r15 = dut.regs[15];

initial begin
    tb_open;
    if ($test$plusargs("vcd"))
        $dumpvars(1, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15);
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h\n",
                   label, reset, areg, breg, writes_dreg, dreg, dreg_value) == 7) begin
        @(posedge clk) $fwrite(outputs_fd, "%h %h\n", areg_value, breg_value);
        @(negedge clk);
    end
    tb_close;
end

endmodule
