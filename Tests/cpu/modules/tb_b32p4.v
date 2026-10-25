// The testbench of the ISS comparison. It runs a program on b32p4 with memory models
// that answer after a set or a random number of cycles, and writes the retire port to a
// trace file.
//
// A trace line is the cycle number, then the record in the format of the ISS trace
// (b32sim's Retired), so pytest can compare the two line by line. The testbench stops
// after the first retired HALT, and prints how it ended as its last line.
//
// Plusargs:
//
//   +rom=<file>        the program as hex words ($readmemh), 1024 of them (required)
//   +trace=<file>      the retire trace (required)
//   +vcd=<file>        also dump a waveform
//   +max_cycles=<n>    give up after n cycles (default 100000)
//   +fetch_latency=<l> how many cycles the instruction memory waits before it answers a
//                      request: a number, or rand for a random number (default 0)
//   +data_latency=<l>  the same for the data memory
//   +seed=<n>          the seed of the random latencies (default 0)
//
// The instruction memory is the ROM (4 KiB at 0x1E000000), and the data memory is the
// whole SDRAM (64 MiB at 0). With a latency of 0, they have the timing of a BRAM: the
// word of a request is there in the cycle after it. With a larger latency, a memory
// answers that many cycles later, and its word is X until then, so a CPU that uses the
// word too early gets X in the trace.
`timescale 1ns / 1ps
module tb_b32p4;

localparam HALT = 32'hFFFFFFFF;

reg clk = 0;
always #5 clk = !clk;

reg reset = 1;

// ---- The CPU ----

wire [31:0] fetch_addr;
wire        fetch_en;
reg  [31:0] fetch_word;
wire        fetch_ready;

wire [31:0] data_addr;
wire        data_req;
wire [31:0] data_write_data;
wire [3:0]  data_byte_enable;
wire        sdram_write;
reg  [31:0] sdram_word;
wire        data_ready;

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
wire [2:0]  retire_irq_id;

b32p4 dut (
    .clk(clk),
    .reset(reset),
    .fetch_addr(fetch_addr),
    .fetch_en(fetch_en),
    .fetch_word(fetch_word),
    .fetch_ready(fetch_ready),
    .data_addr(data_addr),
    .data_req(data_req),
    .data_write_data(data_write_data),
    .data_byte_enable(data_byte_enable),
    .sdram_write(sdram_write),
    .sdram_word(sdram_word),
    .data_ready(data_ready),
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

// ---- The latency of the memories ----

localparam RANDOM = -1;

integer fetch_latency = 0;  // a number of cycles, or RANDOM
integer data_latency  = 0;
integer fetch_seed;
integer data_seed;

// The number of cycles a memory waits before it answers a request, for a latency
// setting and a random number. A random latency is 0 for half of the requests, as for a
// BRAM, 1 to 4 cycles for most others, and 8 to 39 cycles for one in sixteen, as for an
// SDRAM access.
function integer wait_cycles(input integer latency, input [31:0] r);
    if (latency != RANDOM)
        wait_cycles = latency;
    else if (r[0])
        wait_cycles = 0;
    else if (r[3:1] != 3'd0)
        wait_cycles = 1 + r[5:4];
    else
        wait_cycles = 8 + r[9:5];
endfunction

// ---- The instruction memory ----

reg [31:0] rom [0:1023];
reg [9:0]  fetch_index;    // the word of the last request
integer    fetch_wait = 0;  // the cycles until the memory answers it
integer    fetch_cycles;

assign fetch_ready = fetch_wait == 0;

// A request during reset replaces one that hasn't been answered yet. Otherwise a request
// before the answer is a protocol error.
always @(posedge clk) begin
    if (fetch_en) begin
        if (!fetch_ready && !reset) begin
            $display("protocol error in cycle %0d: a fetch request before the last answer",
                     cycle);
            stop;
        end
        fetch_cycles = wait_cycles(fetch_latency, $random(fetch_seed));
        fetch_index <= fetch_addr[11:2];
        fetch_wait  <= fetch_cycles;
        fetch_word  <= fetch_cycles == 0 ? rom[fetch_addr[11:2]] : 32'bx;
    end else if (fetch_wait != 0) begin
        fetch_wait <= fetch_wait - 1;
        if (fetch_wait == 1)
            fetch_word <= rom[fetch_index];
    end
end

// ---- The data memory ----

// A 2-state array starts at 0, like the SDRAM of the ISS, and Icarus keeps 64 MiB of it
// small and fast.
bit [31:0] sdram [0:(1 << 24) - 1];
reg [23:0] data_index;
integer    data_wait = 0;
integer    data_cycles;
integer    lane;

assign data_ready = data_wait == 0;

always @(posedge clk) begin
    if (reset) begin
        data_wait <= 0;
    end else if (data_req) begin
        if (!data_ready) begin
            $display("protocol error in cycle %0d: a data request before the last answer",
                     cycle);
            stop;
        end
        data_cycles = wait_cycles(data_latency, $random(data_seed));
        data_index <= data_addr[25:2];
        data_wait  <= data_cycles;
        sdram_word <= data_cycles == 0 ? sdram[data_addr[25:2]] : 32'bx;
    end else if (data_wait != 0) begin
        data_wait <= data_wait - 1;
        if (data_wait == 1)
            sdram_word <= sdram[data_index];
    end
    if (sdram_write) begin
        if (!data_ready) begin
            $display("protocol error in cycle %0d: a write before the data memory answered",
                     cycle);
            stop;
        end
        for (lane = 0; lane < 4; lane = lane + 1)
            if (data_byte_enable[lane])
                sdram[data_addr[25:2]][8*lane +: 8] <= data_write_data[8*lane +: 8];
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

reg [8*256-1:0] path;
integer max_cycles;

reg [8*8-1:0] latency;

initial begin
    if (!$value$plusargs("rom=%s", path))
        $fatal(1, "+rom=<file> is missing");
    $readmemh(path, rom);
    if (!$value$plusargs("trace=%s", path))
        $fatal(1, "+trace=<file> is missing");
    trace_fd = $fopen(path, "w");
    if (!$value$plusargs("max_cycles=%d", max_cycles))
        max_cycles = 100000;
    if ($value$plusargs("fetch_latency=%s", latency) && latency == "rand")
        fetch_latency = RANDOM;
    else if (!$value$plusargs("fetch_latency=%d", fetch_latency))
        fetch_latency = 0;
    if ($value$plusargs("data_latency=%s", latency) && latency == "rand")
        data_latency = RANDOM;
    else if (!$value$plusargs("data_latency=%d", data_latency))
        data_latency = 0;
    if (!$value$plusargs("seed=%d", fetch_seed))
        fetch_seed = 0;
    data_seed = fetch_seed + 1;
    if ($value$plusargs("vcd=%s", path)) begin
        $dumpfile(path);
        $dumpvars(0, tb_b32p4);
    end
    repeat (2) @(posedge clk);
    reset <= 0;
end

// ---- The trace ----

integer retired = 0;

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
            if (retire_instr == HALT && !retire_irq) begin
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
    end
end

endmodule
