// The testbench of the ISS comparison. It runs a program on b32p4 with the ROM and the
// SDRAM line model behind the cache controller. It raises the interrupt inputs as a
// schedule says, and writes the retire port to a trace file.
//
// A trace line is the cycle number, then the record in the format of the ISS trace
// (b32sim's Retired), so pytest can compare the two line by line. The testbench stops
// after the first retired HALT, and prints how it ended as its last line.
//
// The parameter LINES is the number of lines of each L1 cache.
//
// Plusargs, besides those of sdram_line_model.vh (+sdram_latency, +seed, +sdram_log):
//
//   +rom=<file>        the program as hex words ($readmemh), 1024 of them (required)
//   +sdram=<file>      hex words for the start of the SDRAM, such as a program that runs
//                      from there
//   +interrupts=<file> the interrupt schedule: a line "c n v" sets interrupt input n to
//                      v at the end of cycle c, in the order of the cycles
//   +trace=<file>      the retire trace (required)
//   +vcd=<file>        also dump a waveform
//   +max_cycles=<n>    give up after n cycles (default 100000)
//   +reset_at=<c>      raise reset at the end of cycle c for two cycles, as at the start.
//                      The CPU then starts again, and the trace gets a line "<c> reset"
//                      between the instructions before and after the reset. A line of
//                      output says how many retired before.
//   +sdram_dump=<file> after the HALT, write the SDRAM words of DUMP_BYTES at DUMP_BASE,
//                      where the random programs keep their data, one per line in hex
//
// The ROM (4 KiB at 0x1E000000) is a BRAM with a clock enable. Every other fetch, and
// every read and write of SDRAM (64 MiB at 0), goes through the L1 caches, and from
// there through the line model.
`timescale 1ns / 1ps
module tb_b32p4 #(
    parameter LINES = 128
);

localparam HALT = 32'hFFFFFFFF;

localparam DUMP_BASE  = 32'h10000;  // the SDRAM that +sdram_dump writes out
localparam DUMP_BYTES = 32'h2000;

reg clk = 0;
always #5 clk = !clk;

reg reset = 1;

// Cycle 0 is the first cycle after reset.
integer cycle = 0;
integer trace_fd;

// Ends the simulation. The last line printed before it says how it ended.
task stop;
    begin
        $fclose(trace_fd);
        $finish(0);
    end
endtask

reg [8*256-1:0] trace_path;
reg [8*256-1:0] dump_path;
integer         dump_fd;
integer         dump_i;

// Writes out the SDRAM of DUMP_BASE, if +sdram_dump asks for it
task dump_sdram;
    if ($value$plusargs("sdram_dump=%s", dump_path)) begin
        dump_fd = $fopen(dump_path, "w");
        for (dump_i = 0; dump_i < DUMP_BYTES / 4; dump_i = dump_i + 1)
            $fwrite(dump_fd, "%08x\n", sdram[DUMP_BASE / 4 + dump_i]);
        $fclose(dump_fd);
    end
endtask

// The SDRAM, for the line model. A 2-state array starts at 0, like
// the SDRAM of the ISS, and Icarus keeps 64 MiB of it small and fast.
bit [31:0] sdram [0:(1 << 24) - 1];

`include "sdram_line_model.vh"

// ---- The CPU ----

wire [31:0] fetch_addr;
wire        fetch_en;
reg  [31:0] rom_word;

// As many interrupt inputs as the ISS has
localparam INTERRUPTS = 7;

reg  [INTERRUPTS:1] interrupts = {INTERRUPTS{1'b0}};

wire        retire_valid;
wire [31:0] retire_pc;
wire [31:0] retire_instr;
wire        retire_writes_dreg;
wire [3:0]  retire_dreg;
wire [31:0] retire_dreg_value;
wire        retire_write;
wire [31:0] retire_write_addr;
wire [31:0] retire_write_data;
wire [3:0]  retire_write_be;
wire        retire_push;
wire [31:0] retire_push_value;
wire        retire_pop;
wire [31:0] retire_pop_value;
wire        retire_redirect;
wire [31:0] retire_target;
wire        retire_irq;
wire [7:0]  retire_irq_id;

b32p4 #(
    .INTERRUPTS(INTERRUPTS),
    .L1I_LINES(LINES),
    .L1D_LINES(LINES)
) dut (
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
    .retire_valid(retire_valid),
    .retire_pc(retire_pc),
    .retire_instr(retire_instr),
    .retire_writes_dreg(retire_writes_dreg),
    .retire_dreg(retire_dreg),
    .retire_dreg_value(retire_dreg_value),
    .retire_write(retire_write),
    .retire_write_addr(retire_write_addr),
    .retire_write_data(retire_write_data),
    .retire_write_be(retire_write_be),
    .retire_push(retire_push),
    .retire_push_value(retire_push_value),
    .retire_pop(retire_pop),
    .retire_pop_value(retire_pop_value),
    .retire_redirect(retire_redirect),
    .retire_target(retire_target),
    .retire_irq(retire_irq),
    .retire_irq_id(retire_irq_id)
);

// ---- The ROM ----

reg [31:0] rom [0:1023];

always @(posedge clk)
    if (fetch_en)
        rom_word <= rom[fetch_addr[11:2]];

// IF never moves on with an L1I output that isn't current. That's checked here against
// the SDRAM itself: an instruction from outside the ROM that IF passes to ID must be the
// SDRAM's word at the PC.
always @(posedge clk) begin
    if (!reset && !dut.stall_if && !dut.if_stage.in_rom &&
        dut.if_instr !== sdram[dut.pc[25:2]]) begin
        $display("IF moved on in cycle %0d with %08x at %08x, where the SDRAM has %08x",
                 cycle, dut.if_instr, dut.pc, sdram[dut.pc[25:2]]);
        stop;
    end
end

// ---- The interrupt inputs ----

integer interrupts_fd = 0;
integer next_cycle = -1;  // of the next line of the schedule, -1 at the end
integer next_number;
integer next_value;

task read_schedule;
    if ($fscanf(interrupts_fd, "%d %d %d\n", next_cycle, next_number, next_value) != 3)
        next_cycle = -1;
endtask

always @(posedge clk) begin
    while (!reset && next_cycle == cycle) begin
        interrupts[next_number] <= next_value[0];
        read_schedule;
    end
end

// ---- The registers as wires for the waveform, because Icarus doesn't dump arrays ----

wire [31:0] r1 = dut.id_stage.regfile.regs[1], r2 = dut.id_stage.regfile.regs[2],
            r3 = dut.id_stage.regfile.regs[3], r4 = dut.id_stage.regfile.regs[4],
            r5 = dut.id_stage.regfile.regs[5], r6 = dut.id_stage.regfile.regs[6],
            r7 = dut.id_stage.regfile.regs[7], r8 = dut.id_stage.regfile.regs[8],
            r9 = dut.id_stage.regfile.regs[9], r10 = dut.id_stage.regfile.regs[10],
            r11 = dut.id_stage.regfile.regs[11], r12 = dut.id_stage.regfile.regs[12],
            r13 = dut.id_stage.regfile.regs[13], r14 = dut.id_stage.regfile.regs[14],
            r15 = dut.id_stage.regfile.regs[15];

// ---- Start ----

// The registers are undefined after a reset, so they start with a junk pattern instead
// of 0. A program that reads a register before it writes it then differs from the ISS,
// which stops on such a read in strict mode, instead of passing by luck.
integer r;

initial
    for (r = 1; r < 16; r = r + 1)
        dut.id_stage.regfile.regs[r] = 32'hDEAD0000 | r;

// Reads hex words into the start of the SDRAM. $readmemh would go through all of the
// SDRAM, which takes a lot longer than the run.
task load_sdram(input [8*256-1:0] path);
    integer fd;
    integer words;
    reg [31:0] word;
    begin
        fd = $fopen(path, "r");
        if (fd == 0)
            $fatal(1, "cannot read %0s", path);
        for (words = 0; $fscanf(fd, "%h\n", word) == 1; words = words + 1)
            sdram[words] = word;
        $fclose(fd);
    end
endtask

reg [8*256-1:0] path;
integer max_cycles;

initial begin
    if (!$value$plusargs("rom=%s", path))
        $fatal(1, "+rom=<file> is missing");
    $readmemh(path, rom);
    if ($value$plusargs("sdram=%s", path))
        load_sdram(path);
    if ($value$plusargs("interrupts=%s", path)) begin
        interrupts_fd = $fopen(path, "r");
        if (interrupts_fd == 0)
            $fatal(1, "cannot read %0s", path);
        read_schedule;
    end
    if (!$value$plusargs("trace=%s", trace_path))
        $fatal(1, "+trace=<file> is missing");
    trace_fd = $fopen(trace_path, "w");
    if (!$value$plusargs("reset_at=%d", reset_at))
        reset_at = -1;
    if (!$value$plusargs("max_cycles=%d", max_cycles))
        max_cycles = 100000;
    if ($value$plusargs("vcd=%s", path)) begin
        $dumpfile(path);
        $dumpvars(0, tb_b32p4);
    end
    repeat (2) @(posedge clk);
    reset <= 0;
end

// ---- The trace, and a reset in the middle of the run ----

integer    retired = 0;
reg [31:0] last_pc = 32'd0;  // of the last instruction that retired
integer    reset_at;
integer    reset_left = 0;   // the cycles that a reset in the middle of the run stays 1

always @(posedge clk) begin
    if (!reset) begin
        if (retire_valid) begin
            $fwrite(trace_fd, "%0d %08x %08x", cycle, retire_pc, retire_instr);
            if (retire_irq)
                $fwrite(trace_fd, " irq %0d", retire_irq_id);
            if (retire_writes_dreg)
                $fwrite(trace_fd, " r%0d=%08x", retire_dreg, retire_dreg_value);
            if (retire_write)
                $fwrite(trace_fd, " write %08x=%08x/%0x",
                        retire_write_addr, retire_write_data, retire_write_be);
            if (retire_push)
                $fwrite(trace_fd, " push %08x", retire_push_value);
            if (retire_pop)
                $fwrite(trace_fd, " pop %08x", retire_pop_value);
            if (retire_redirect)
                $fwrite(trace_fd, " -> %08x", retire_target);
            $fwrite(trace_fd, "\n");
            retired = retired + 1;
            last_pc = retire_pc;
            if (retire_instr == HALT && !retire_irq) begin
                dump_sdram;
                $display("halted after %0d cycles and %0d instructions",
                         cycle + 1, retired);
                stop;
            end
        end
        if (cycle + 1 == max_cycles) begin
            $display("timeout: no HALT within %0d cycles", max_cycles);
            stop;
        end
        cycle <= cycle + 1;

        // After the instruction that retires in that cycle, the trace marks the reset.
        if (cycle == reset_at) begin
            $display("reset in cycle %0d, after %0d instructions, the last at %08x",
                     cycle, retired, last_pc);
            $fwrite(trace_fd, "%0d reset\n", cycle);
            reset      <= 1'b1;
            reset_left = 2;
            retired    = 0;
        end
    end else if (reset_left > 0) begin
        reset_left = reset_left - 1;
        if (reset_left == 0)
            reset <= 1'b0;
    end
end

endmodule
