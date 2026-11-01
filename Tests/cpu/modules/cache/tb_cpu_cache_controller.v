// Testbench of cpu_cache_controller, with both caches and the SDRAM line model.
//
// It plays a list of steps. A step is a fetch through the L1I, an access (a read or a
// write) through the L1D, or both, each starting a number of cycles after the step
// starts. The testbench does what IF and MEM will do: it reads the cache, and if the
// output is current and the line misses, it sets the request (for the L1D with the
// victim from the tag it read) and keeps it until the controller takes it. Once the
// cache's writing signal is 0 again, it reads again. A read or fetch then gives the word,
// and a write writes the word and the entry {valid, dirty, tag} through port A. The next
// step starts when both are done.
//
// A step can also be a CCACHE or CCACHED, which sets the clear request as MEM does, keeps
// it until the controller takes it, and waits until l1d_writing is 0 again, or a reset of
// two cycles, which waits for the walk after it in the same way. Either is the only thing
// in its step. The controller also walks through the caches after the first reset, and
// the first step starts after that.
//
// An output is current if the cache's writing signal was 0 in the cycle of the read. The
// testbench only reads a cache when its fetch or access starts and after its own fill,
// so its outputs are always current here; the pipeline needs the rule for more.
//
// Plusargs, besides those of sdram_line_model.vh:
//
//   +steps=<file>        the steps, one per line in hex columns: a label of up to 12
//                        ASCII characters (as in tb_unit.vh), fetch (0 or 1), the fetch
//                        address, its start, access (0 none, 1 read, 2 write, 3 CCACHE,
//                        4 CCACHED, 5 reset), the access address, the write data, the
//                        byte enables, its start. The starts are in cycles after the
//                        step starts.
//   +sdram_words=<file>  "index word" lines in hex, written into the SDRAM before the
//                        start (the rest of the SDRAM is 0)
//   +outputs=<file>      per step the fetched word and the read word (0 without them)
//   +contents=<file>     at the end, per line of the L1I and then of the L1D: the entry
//                        and the 8 words
//   +vcd=<file>          also dump a waveform
`timescale 1ns / 1ps
module tb_cpu_cache_controller #(
    parameter L1I_LINES = 128,
    parameter L1D_LINES = 128
);

localparam I_INDEX = $clog2(L1I_LINES);
localparam D_INDEX = $clog2(L1D_LINES);

reg clk = 0;
always #5 clk = !clk;

reg reset = 1;

// Cycle 0 is the first cycle after reset.
integer cycle = 0;

always @(posedge clk)
    if (!reset)
        cycle <= cycle + 1;

integer steps_fd;
integer outputs_fd;
integer contents_fd;

reg [8*12-1:0] label;  // the step, shown as ASCII in the preset

task stop;
    begin
        $fclose(outputs_fd);
        $finish(0);
    end
endtask

// The SDRAM, 64 MiB at 0, as in tb_b32p4.v
bit [31:0] sdram [0:(1 << 24) - 1];

`include "sdram_line_model.vh"

// ---- The caches and the controller ----

reg                  l1i_request = 1'b0;
reg  [25:2]          l1i_addr;
wire                 l1i_take;
wire                 l1i_writing;
reg  [I_INDEX+4:2]   l1i_a_addr;
reg                  l1i_a_enable = 1'b0;
wire [31:0]          l1i_a_word;
wire [21-I_INDEX:0]  l1i_a_entry;

reg                  l1d_request = 1'b0;
reg  [25:2]          l1d_addr;
reg                  victim_dirty;
reg  [20-D_INDEX:0]  victim_tag;
wire                 l1d_take;
wire                 l1d_writing;
reg  [D_INDEX+4:2]   l1d_a_addr;
wire [31:0]          l1d_a_word;
wire [22-D_INDEX:0]  l1d_a_entry;
reg                  l1d_a_write = 1'b0;
reg  [3:0]           l1d_a_byte_enable;
reg  [31:0]          l1d_a_write_data;
reg                  l1d_a_entry_write = 1'b0;
reg  [22-D_INDEX:0]  l1d_a_entry_write_value;

reg                  clear_request = 1'b0;
reg                  clear_data_only;
wire                 clear_take;

wire [31:0]          b_write_data;
wire [I_INDEX+4:2]   l1i_b_addr;
wire                 l1i_b_write;
wire                 l1i_b_entry_write;
wire [21-I_INDEX:0]  l1i_b_entry_write_value;
wire [D_INDEX+4:2]   l1d_b_addr;
wire [31:0]          l1d_b_word;
wire [22-D_INDEX:0]  l1d_b_entry;
wire                 l1d_b_write;
wire                 l1d_b_entry_write;
wire [22-D_INDEX:0]  l1d_b_entry_write_value;

wire                 l1i_filling;
wire                 l1d_filling;
wire [25:2]          fill_addr;

cpu_l1i #(
    .LINES(L1I_LINES)
) l1i (
    .clk(clk),
    .a_addr(l1i_a_addr),
    .a_enable(l1i_a_enable),
    .a_word(l1i_a_word),
    .a_entry(l1i_a_entry),
    .b_addr(l1i_b_addr),
    .b_write(l1i_b_write),
    .b_write_data(b_write_data),
    .b_entry_write(l1i_b_entry_write),
    .b_entry_write_value(l1i_b_entry_write_value)
);

cpu_l1d #(
    .LINES(L1D_LINES)
) l1d (
    .clk(clk),
    .a_addr(l1d_a_addr),
    .a_word(l1d_a_word),
    .a_entry(l1d_a_entry),
    .a_write(l1d_a_write),
    .a_byte_enable(l1d_a_byte_enable),
    .a_write_data(l1d_a_write_data),
    .a_entry_write(l1d_a_entry_write),
    .a_entry_write_value(l1d_a_entry_write_value),
    .b_addr(l1d_b_addr),
    .b_word(l1d_b_word),
    .b_entry(l1d_b_entry),
    .b_write(l1d_b_write),
    .b_write_data(b_write_data),
    .b_entry_write(l1d_b_entry_write),
    .b_entry_write_value(l1d_b_entry_write_value)
);

cpu_cache_controller #(
    .L1I_LINES(L1I_LINES),
    .L1D_LINES(L1D_LINES)
) dut (
    .clk(clk),
    .reset(reset),
    .l1i_request(l1i_request),
    .l1i_addr(l1i_addr),
    .l1i_take(l1i_take),
    .l1i_writing(l1i_writing),
    .l1d_request(l1d_request),
    .l1d_addr(l1d_addr),
    .victim_dirty(victim_dirty),
    .victim_tag(victim_tag),
    .l1d_take(l1d_take),
    .l1d_writing(l1d_writing),
    .l1i_filling(l1i_filling),
    .l1d_filling(l1d_filling),
    .fill_addr(fill_addr),
    .clear_request(clear_request),
    .clear_data_only(clear_data_only),
    .clear_take(clear_take),
    .b_write_data(b_write_data),
    .l1i_b_addr(l1i_b_addr),
    .l1i_b_write(l1i_b_write),
    .l1i_b_entry_write(l1i_b_entry_write),
    .l1i_b_entry_write_value(l1i_b_entry_write_value),
    .l1d_b_addr(l1d_b_addr),
    .l1d_b_word(l1d_b_word),
    .l1d_b_entry(l1d_b_entry),
    .l1d_b_write(l1d_b_write),
    .l1d_b_entry_write(l1d_b_entry_write),
    .l1d_b_entry_write_value(l1d_b_entry_write_value),
    .sdram_req(sdram_req),
    .sdram_write(sdram_write),
    .sdram_line(sdram_line),
    .sdram_first(sdram_first),
    .sdram_take(sdram_take),
    .sdram_read_valid(sdram_read_valid),
    .sdram_read_word(sdram_read_word),
    .sdram_next(sdram_next),
    .sdram_write_word(sdram_write_word)
);

// The word that arrives during a fill is the SDRAM's word at fill_addr, which IF and MEM
// use for early restart.
always @(posedge clk)
    if (!reset && sdram_read_valid && sdram_read_word !== sdram[fill_addr]) begin
        $display("the word that arrived in cycle %0d isn't the one at fill_addr %06x",
                 cycle, fill_addr);
        stop;
    end

// ---- IF and MEM ----

// The testbench changes its signals at the falling edge, half a cycle before the rising
// edge where the caches and the controller see them.

reg [31:0] fetched;    // the word of the fetch of the step
reg [31:0] read_word;  // the word of the read of the step

task fetch(input [25:2] addr, input integer start);
    reg current;
    reg done;
    begin
        repeat (start) @(negedge clk);
        done = 1'b0;
        while (!done) begin
            l1i_a_addr   = addr[I_INDEX+4:2];
            l1i_a_enable = 1'b1;
            current      = !l1i_writing;
            @(negedge clk);
            l1i_a_enable = 1'b0;
            if (current && l1i_a_entry == {1'b1, addr[25:I_INDEX+5]}) begin
                fetched = l1i_a_word;
                done    = 1'b1;
            end else begin
                if (current) begin
                    l1i_request = 1'b1;
                    l1i_addr    = addr;
                    while (!l1i_take)
                        @(negedge clk);
                    l1i_request = 1'b0;
                end
                while (l1i_writing)
                    @(negedge clk);
            end
        end
    end
endtask

task access(input write, input [25:2] addr, input [31:0] data, input [3:0] byte_enable,
            input integer start);
    reg current;
    reg done;
    begin
        repeat (start) @(negedge clk);
        done = 1'b0;
        while (!done) begin
            l1d_a_addr = addr[D_INDEX+4:2];
            current    = !l1d_writing;
            @(negedge clk);
            if (current && l1d_a_entry[22-D_INDEX] &&
                l1d_a_entry[20-D_INDEX:0] == addr[25:D_INDEX+5]) begin
                if (write) begin
                    l1d_a_write             = 1'b1;
                    l1d_a_byte_enable       = byte_enable;
                    l1d_a_write_data        = data;
                    l1d_a_entry_write       = 1'b1;
                    l1d_a_entry_write_value = {2'b11, addr[25:D_INDEX+5]};
                    @(negedge clk);
                    l1d_a_write       = 1'b0;
                    l1d_a_entry_write = 1'b0;
                end else begin
                    read_word = l1d_a_word;
                end
                done = 1'b1;
            end else begin
                if (current) begin
                    l1d_request  = 1'b1;
                    l1d_addr     = addr;
                    victim_dirty = l1d_a_entry[22-D_INDEX] && l1d_a_entry[21-D_INDEX];
                    victim_tag   = l1d_a_entry[20-D_INDEX:0];
                    while (!l1d_take)
                        @(negedge clk);
                    l1d_request = 1'b0;
                end
                while (l1d_writing)
                    @(negedge clk);
            end
        end
    end
endtask

// CCACHE or CCACHED, as MEM makes it: the request until the take, then the walk
task clear(input data_only, input integer start);
    begin
        repeat (start) @(negedge clk);
        clear_request   = 1'b1;
        clear_data_only = data_only;
        @(negedge clk);
        while (!clear_take)
            @(negedge clk);
        clear_request = 1'b0;
        while (l1d_writing)
            @(negedge clk);
    end
endtask

// A reset of two cycles, as at the start, and the walk after it
task pulse_reset(input integer start);
    begin
        repeat (start) @(negedge clk);
        reset = 1'b1;
        repeat (2) @(negedge clk);
        reset = 1'b0;
        while (l1d_writing)
            @(negedge clk);
    end
endtask

// ---- Start ----

reg [8*256-1:0] path;
reg             do_fetch;
reg [31:0]      fetch_addr;
integer         fetch_start;
reg [2:0]       kind;
reg [31:0]      data_addr;
reg [31:0]      write_data;
reg [3:0]       byte_enable;
integer         data_start;
integer         sdram_fd;
reg [31:0]      index;
reg [31:0]      word;
integer         i;
integer         w;

initial begin
    if (!$value$plusargs("steps=%s", path))
        $fatal(1, "+steps=<file> is missing");
    steps_fd = $fopen(path, "r");
    if (steps_fd == 0)
        $fatal(1, "cannot read %0s", path);
    if (!$value$plusargs("outputs=%s", path))
        $fatal(1, "+outputs=<file> is missing");
    outputs_fd = $fopen(path, "w");
    if (!$value$plusargs("contents=%s", path))
        $fatal(1, "+contents=<file> is missing");
    contents_fd = $fopen(path, "w");
    if ($value$plusargs("sdram_words=%s", path)) begin
        sdram_fd = $fopen(path, "r");
        if (sdram_fd == 0)
            $fatal(1, "cannot read %0s", path);
        while ($fscanf(sdram_fd, "%h %h\n", index, word) == 2)
            sdram[index] = word;
        $fclose(sdram_fd);
    end
    if ($value$plusargs("vcd=%s", path)) begin
        $dumpfile(path);
        $dumpvars(0, tb_cpu_cache_controller);
    end
    label       = "reset";
    l1i_a_addr  = 0;
    l1d_a_addr  = 0;
    repeat (2) @(posedge clk);
    reset <= 1'b0;
    @(negedge clk);
    while (l1d_writing)
        @(negedge clk);

    while ($fscanf(steps_fd, "%h %h %h %h %h %h %h %h %h\n", label, do_fetch, fetch_addr,
                   fetch_start, kind, data_addr, write_data, byte_enable, data_start) == 9)
    begin
        fetched   = 32'd0;
        read_word = 32'd0;
        if (kind == 3'd3 || kind == 3'd4)
            clear(kind == 3'd4, data_start);
        else if (kind == 3'd5)
            pulse_reset(data_start);
        else fork
            if (do_fetch)
                fetch(fetch_addr[25:2], fetch_start);
            if (kind != 3'd0)
                access(kind == 3'd2, data_addr[25:2], write_data, byte_enable, data_start);
        join
        $fwrite(outputs_fd, "%h %h\n", fetched, read_word);
    end
    label = "end";
    @(negedge clk);

    for (i = 0; i < L1I_LINES; i = i + 1) begin
        $fwrite(contents_fd, "%h", l1i.tags[i]);
        for (w = 0; w < 8; w = w + 1)
            $fwrite(contents_fd, " %h", l1i.data[8*i + w]);
        $fwrite(contents_fd, "\n");
    end
    for (i = 0; i < L1D_LINES; i = i + 1) begin
        $fwrite(contents_fd, "%h", l1d.tags[i]);
        for (w = 0; w < 8; w = w + 1)
            $fwrite(contents_fd, " %h", {l1d.lane3[8*i + w], l1d.lane2[8*i + w],
                                         l1d.lane1[8*i + w], l1d.lane0[8*i + w]});
        $fwrite(contents_fd, "\n");
    end
    $fclose(contents_fd);
    stop;
end

endmodule
