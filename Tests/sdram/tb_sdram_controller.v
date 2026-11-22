// Testbench of the SDRAM controller, sdram_controller, with two chip models (sdram_chip)
// as the board's 32-bit SDRAM, and a driver that plays the CPU's cache controller on the
// line port.
//
// The chips get the clock shifted by 7.9 ns (sdram decision 6), without the delays of
// the pins and the board's traces, which the timing analysis checks. The pause after
// power up and after a reset is 1 us here instead of 200 us.
//
// The driver sends random reads (from every first word) and writes of a pool of 16
// lines, with random gaps, also back to back. It keeps a reference copy of the lines,
// and checks every word read in its order. It also checks the line port's protocol: a
// take only for a request, 8 words for a read, 8 sdram_next for a write, and no request
// waiting forever. It stops at the first error of the chips or of its own checks.
//
// Plusargs:
//
//   +requests=<n>  the number of requests (default 1000)
//   +seed=<n>      the seed of the random traffic (default 0)
//   +resets=1      also resets the controller (and the driver, as the system's reset
//                  does) at random moments, also during a read or a write. The line of
//                  a write that a reset cuts off isn't checked until it's written again.
//   +demo=1        instead of random traffic: a write, a read of the same line from word
//                  5, and the same for a second line
//   +vcd=<file>    also dump a waveform
//
// At the end it writes "done: <reads> reads, <writes> writes, <refreshes> refreshes,
// <resets> resets".
`timescale 1ns / 1ps
module tb_sdram_controller;

parameter PAUSE_CYCLES = 100;

localparam LINES = 16;

reg clk = 1'b0;
always #5 clk = !clk;

reg sdram_clk = 1'b0;
always @(clk)
    sdram_clk <= #7.9 clk;

integer cycle = 0;
always @(posedge clk)
    cycle <= cycle + 1;

reg reset = 1'b1;

// ---- The controller and the chips ----

reg         sdram_req = 1'b0;
reg         sdram_write = 1'b0;
reg  [25:5] sdram_line = 21'd0;
reg  [2:0]  sdram_first = 3'd0;
wire        sdram_take;
wire        sdram_read_valid;
wire [31:0] sdram_read_word;
wire        sdram_next;
reg  [31:0] sdram_write_word = 32'bx;

wire        cs_n;
wire        ras_n;
wire        cas_n;
wire        we_n;
wire        cke;
wire [12:0] a;
wire [1:0]  ba;
wire [3:0]  dqm;
wire [31:0] dq;
wire [4:0]  error_lo;
wire [4:0]  error_hi;

sdram_controller #(
    .PAUSE_CYCLES(PAUSE_CYCLES)
) dut (
    .clk(clk),
    .reset(reset),
    .sdram_req(sdram_req),
    .sdram_write(sdram_write),
    .sdram_line(sdram_line),
    .sdram_first(sdram_first),
    .sdram_take(sdram_take),
    .sdram_read_valid(sdram_read_valid),
    .sdram_read_word(sdram_read_word),
    .sdram_next(sdram_next),
    .sdram_write_word(sdram_write_word),
    .sdram_cs_n(cs_n),
    .sdram_ras_n(ras_n),
    .sdram_cas_n(cas_n),
    .sdram_we_n(we_n),
    .sdram_cke(cke),
    .sdram_a(a),
    .sdram_ba(ba),
    .sdram_dqm(dqm),
    .sdram_dq(dq)
);

sdram_chip #(
    .PAUSE(PAUSE_CYCLES * 10_000)
) chip_lo (
    .clk(sdram_clk),
    .cke(cke),
    .cs_n(cs_n),
    .ras_n(ras_n),
    .cas_n(cas_n),
    .we_n(we_n),
    .ba(ba),
    .a(a),
    .dqm(dqm[1:0]),
    .dq(dq[15:0]),
    .error(error_lo)
);

sdram_chip #(
    .PAUSE(PAUSE_CYCLES * 10_000)
) chip_hi (
    .clk(sdram_clk),
    .cke(cke),
    .cs_n(cs_n),
    .ras_n(ras_n),
    .cas_n(cas_n),
    .we_n(we_n),
    .ba(ba),
    .a(a),
    .dqm(dqm[3:2]),
    .dq(dq[31:16]),
    .error(error_hi)
);

// The command on the pins, as text for the waveform
reg [8*8-1:0] command;
always @*
    if (cs_n)
        command = "";
    else
        case ({ras_n, cas_n, we_n})
            3'b011:  command = "ACT";
            3'b101:  command = "READ";
            3'b100:  command = "WRITE";
            3'b010:  command = a[10] ? "PRE ALL" : "PRE";
            3'b001:  command = "REFRESH";
            3'b000:  command = "MODE";
            3'b111:  command = "";
            default: command = "?";
        endcase

// ---- The driver ----

integer requests = 1000;
integer seed = 0;
reg     resets_on = 1'b0;
reg     demo = 1'b0;

reg  [25:5] pool [0:LINES-1];          // the lines
reg  [31:0] reference [0:LINES*8-1];   // their words
reg         known [0:LINES-1];         // the line's words are known

integer     sent = 0;                  // requests made
integer     reads = 0;
integer     writes = 0;
integer     refreshes = 0;
integer     resets = 0;
integer     gap = 0;                   // cycles until the next request
integer     waited = 0;                // cycles the request has waited

integer     index;                     // the pool line of the request
reg  [31:0] words [0:7];               // the words of a write request
reg  [31:0] writing [0:7];             // the words of the write that was taken
integer     nexts = 8;                 // sdram_next of that write so far

// The expected words of the reads that were taken, in order
reg  [31:0] expected [0:255];
reg         check [0:255];             // the word is known
integer     head = 0;
integer     tail = 0;

integer     write_taken = -100;        // the cycle of the last write's take
integer     write_index = 0;
integer     reset_left = 0;            // cycles of reset still to come
integer     i;
integer     r;

task stop(input [8*64-1:0] message);
    begin
        $display("error in cycle %0d: %0s", cycle, message);
        $finish(0);
    end
endtask

// A new request: random, or the demo's
task new_request;
    begin
        if (demo) begin
            index       = sent / 2;
            sdram_write <= sent % 2 == 0;
            sdram_first <= sent % 2 == 0 ? 3'd0 : 3'd5;
        end else begin
            index       = $unsigned($random(seed)) % LINES;
            r           = $random(seed);
            sdram_write <= r[0];
            sdram_first <= r[0] ? 3'd0 : r[3:1];
        end
        for (i = 0; i < 8; i = i + 1)
            words[i] = demo ? 32'h11111111 * (i + 1) + sent : $random(seed);
        sdram_req  <= 1'b1;
        sdram_line <= pool[index];
        sent        = sent + 1;
        waited      = 0;
    end
endtask

initial begin
    if (!$value$plusargs("requests=%d", requests))
        requests = 1000;
    if (!$value$plusargs("seed=%d", seed))
        seed = 0;
    resets_on = $test$plusargs("resets");
    demo = $test$plusargs("demo");
    if (demo)
        requests = 4;
    // Unique lines: bits [8:5] are the index, the rest is random.
    for (i = 0; i < LINES; i = i + 1) begin
        r = $random(seed);
        pool[i] = {r[16:0], i[3:0]};
        known[i] = 1'b1;
    end
    for (i = 0; i < LINES * 8; i = i + 1)
        reference[i] = 32'd0;
end

reg [8*256-1:0] vcd_path;
initial
    if ($value$plusargs("vcd=%s", vcd_path)) begin
        $dumpfile(vcd_path);
        $dumpvars(0, tb_sdram_controller);
    end

always @(posedge clk) begin
    sdram_write_word <= 32'bx;

    if (error_lo != 5'd0 || error_hi != 5'd0)
        stop("an SDRAM chip reports an error");
    if (cs_n === 1'b0 && {ras_n, cas_n, we_n} === 3'b001)
        refreshes = refreshes + 1;

    // A reset, now and then
    if (reset_left > 0) begin
        reset_left = reset_left - 1;
        reset <= reset_left > 0;
    end else if (cycle >= 2 && reset) begin
        reset <= 1'b0;
    end else if (resets_on && $unsigned($random(seed)) % 600 == 0) begin
        reset_left = 1 + $unsigned($random(seed)) % 3;
        reset <= 1'b1;
        resets = resets + 1;
        chip_lo.pause_refresh_check;
        chip_hi.pause_refresh_check;
    end

    if (reset) begin
        // The driver resets with the system: the requests and words in flight are gone.
        if (cycle - write_taken < 16)
            known[write_index] = 1'b0;
        sdram_req <= 1'b0;
        nexts      = 8;
        head       = tail;
        gap        = 0;
    end else begin
        // The line port's protocol
        if (sdram_take && !sdram_req)
            stop("sdram_take without a request");
        if (sdram_next && nexts == 8 && !(sdram_take && sdram_write))
            stop("sdram_next without a write");
        if (sdram_read_valid && head == tail)
            stop("sdram_read_valid without a read");

        if (sdram_take) begin
            sdram_req <= 1'b0;
            if (sdram_write) begin
                for (i = 0; i < 8; i = i + 1) begin
                    writing[i] = words[i];
                    reference[index * 8 + i] = words[i];
                end
                known[index] = 1'b1;
                nexts        = 0;
                write_taken  = cycle;
                write_index  = index;
                writes       = writes + 1;
            end else begin
                for (i = 0; i < 8; i = i + 1) begin
                    expected[tail % 256] = reference[index * 8 + (sdram_first + i) % 8];
                    check[tail % 256]    = known[index];
                    tail = tail + 1;
                end
                reads = reads + 1;
            end
            gap = demo ? 4 : $unsigned($random(seed)) % 4;
        end

        if (sdram_next) begin
            sdram_write_word <= writing[nexts];
            nexts = nexts + 1;
        end

        if (sdram_read_valid) begin
            if (check[head % 256] && sdram_read_word !== expected[head % 256]) begin
                $display("read word %h, expected %h", sdram_read_word, expected[head % 256]);
                stop("a word read is wrong");
            end
            head = head + 1;
        end

        // The next request, or the end
        if (!sdram_req || sdram_take) begin
            if (sent < requests) begin
                if (gap == 0)
                    new_request;
                else
                    gap = gap - 1;
            end else if (head == tail && nexts == 8) begin
                $display("done: %0d reads, %0d writes, %0d refreshes, %0d resets",
                         reads, writes, refreshes, resets);
                $finish(0);
            end else begin
                waited = waited + 1;
                if (waited > 1000)
                    stop("the words of a request never come");
            end
        end else begin
            waited = waited + 1;
            if (waited > 2 * PAUSE_CYCLES + 1000)
                stop("a request is never taken");
        end
    end
end

endmodule
