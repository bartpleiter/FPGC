// Testbench of cpu_io_regs. See tb_unit.vh for how the vectors are applied.
//
// The outputs are written at the rising edge, just before it clocks the write, the
// events and the interrupt, because that's what the instruction in MEM sees. One reset
// cycle comes before the vectors, so the registers start from a known state.
`timescale 1ns / 1ps
module tb_cpu_io_regs;

`include "tb_unit.vh"

reg         reset;
reg  [3:2]  addr;
reg         word;
reg         write;
reg  [31:0] write_data;
reg  [7:0]  sp;
reg  [31:0] pc;
reg  [5:0]  events;
reg         take_interrupt;
reg  [7:0]  interrupt_id;
reg         reti;
wire        bad_read;
wire        bad_write;
wire [31:0] read_value;
wire        sp_write;
wire [31:0] pc_backup;
wire        interrupts_enabled;
wire [7:0]  last_interrupt;

cpu_io_regs dut (
    .clk(clk),
    .reset(reset),
    .addr(addr),
    .word(word),
    .bad_read(bad_read),
    .bad_write(bad_write),
    .read_value(read_value),
    .write(write),
    .write_data(write_data),
    .sp(sp),
    .sp_write(sp_write),
    .pc(pc),
    .events(events),
    .take_interrupt(take_interrupt),
    .interrupt_id(interrupt_id),
    .reti(reti),
    .pc_backup(pc_backup),
    .interrupts_enabled(interrupts_enabled),
    .last_interrupt(last_interrupt)
);

initial begin
    tb_open;
    {reset, addr, word, write, write_data, sp, pc, events, take_interrupt, interrupt_id,
     reti} = {1'b1, 92'd0};
    @(negedge clk);
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h %h %h %h %h\n", label, reset,
                   addr, word, write, write_data, sp, pc, events, take_interrupt,
                   interrupt_id, reti) == 12) begin
        @(posedge clk)
            $fwrite(outputs_fd, "%h %h %h %h %h %h %h\n", bad_read, bad_write, read_value,
                    sp_write, pc_backup, interrupts_enabled, last_interrupt);
        @(negedge clk);
    end
    tb_close;
end

endmodule
