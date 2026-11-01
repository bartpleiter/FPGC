// A model of the SDRAM controller on the SDRAM line port of the CPU, for simulation only.
// Include it inside a testbench module, before the instance that connects to the port:
// it declares the port's signals, sdram_req to sdram_write_word. The testbench must
// have `clk`, `reset`, the SDRAM as `bit [31:0] sdram [0:(1 << 24) - 1]` (a word
// array of 64 MiB at address 0), `cycle` (an integer, the number of the current cycle)
// and a task `stop` that ends the simulation.
//
// The model handles one request at a time. It sees a request in a cycle in which
// sdram_req is 1 and it's idle, and takes it at the end of that cycle, so sdram_take is
// 1 in the next cycle. With a latency of N:
//
//   - A read gives the 8 words of the line, the first one N cycles after the cycle it
//     saw the request, starting at word sdram_first and wrapping around. It's idle again
//     in the cycle after the last word.
//   - A write asks for the first word with sdram_next in the cycle of the take, and for
//     the others in the cycles after. It takes each word in the cycle after its
//     sdram_next, and writes it into the SDRAM. It's idle again N + 8 cycles after the
//     cycle it saw the request, but not before the cycle after the last word.
//
// The default latency of 6 is B32P3's SDRAM controller: the first word of a read 6
// cycles after the request is seen, and a write keeps it busy for 14 cycles (ACTIVATE,
// the row delay, the 8 words, and the write and precharge times). A random latency is
// 6 for most requests and 10 to 40 for some, and then there are also random gaps
// between the words, and the first sdram_next of a write can come later.
//
// It checks the protocol: a request must stay the same until it's taken, a write must
// start at word 0, and a word to write must not be X. A protocol error ends the run
// with a "protocol error" line.
//
// Plusargs:
//
//   +sdram_latency=<l>  a number of cycles of at least 1, or rand (default 6)
//   +seed=<n>           the seed of the random latencies (default 0)
//   +sdram_log=<file>   writes a line for each request: "<cycle> read <line> <first>"
//                       when a read is taken, and "<cycle> write <line> <8 words>" when
//                       a write has taken its last word, with the line as bits [25:5]
//                       of the address, in hex

// ---- The SDRAM line port ----

wire        sdram_req;
wire        sdram_write;
wire [25:5] sdram_line;
wire [2:0]  sdram_first;
reg         sdram_take = 1'b0;
reg         sdram_read_valid = 1'b0;
reg  [31:0] sdram_read_word = 32'bx;
reg         sdram_next = 1'b0;
wire [31:0] sdram_write_word;

// ---- The model ----

localparam SDRAM_RANDOM = -1;

integer sdram_latency = 6;  // a number of cycles, or SDRAM_RANDOM
integer sdram_seed = 0;
integer sdram_log_fd = 0;

reg         sdram_busy = 1'b0;    // handles a request
integer     sdram_idle_at = 0;    // the first cycle in which it can see a request
reg         sdram_writes;         // the request is a write
reg  [25:5] sdram_at_line;        // the line of the request
reg  [2:0]  sdram_at_first;
integer     sdram_seen;           // the cycle in which it saw the request
integer     sdram_n;              // the latency of the request
integer     sdram_wait;           // the cycles until the next word or sdram_next
integer     sdram_sent;           // the words given, or asked for with sdram_next
integer     sdram_taken;          // the words of a write taken
reg         sdram_word_due = 1'b0;  // sdram_next was 1 in the cycle before
reg  [31:0] sdram_written [0:7];  // the words of a write, for the log

reg         sdram_waiting = 1'b0;  // a request was there in the cycle before, not taken
reg         sdram_waiting_write;
reg  [25:5] sdram_waiting_line;
reg  [2:0]  sdram_waiting_first;

integer     sdram_r;

// The latency of a request: usually 6, and 10 to 40 for one in four
function integer sdram_request_latency(input integer latency, input [31:0] r);
    if (latency != SDRAM_RANDOM)
        sdram_request_latency = latency;
    else if (r[1:0] != 2'd0)
        sdram_request_latency = 6;
    else
        sdram_request_latency = 10 + r[7:2] % 31;
endfunction

// The gap before the next word: with a random latency, 1 to 3 cycles for one in four
function integer sdram_gap(input integer latency, input [31:0] r);
    if (latency == SDRAM_RANDOM && r[1:0] == 2'd0)
        sdram_gap = 1 + r[3:2] % 3;
    else
        sdram_gap = 0;
endfunction

task sdram_protocol_error(input [8*64-1:0] message);
    begin
        $display("protocol error in cycle %0d: %0s", cycle, message);
        stop;
    end
endtask

always @(posedge clk) begin
    sdram_take       <= 1'b0;
    sdram_read_valid <= 1'b0;
    sdram_read_word  <= 32'bx;
    sdram_next       <= 1'b0;
    sdram_word_due   <= sdram_next;

    if (reset) begin
        sdram_busy     = 1'b0;
        sdram_idle_at  = 0;
        sdram_waiting  = 1'b0;
        sdram_word_due <= 1'b0;
    end else begin
        // A request that waits must stay the same.
        if (sdram_waiting && !sdram_req)
            sdram_protocol_error("an SDRAM request was dropped before it was taken");
        if (sdram_waiting && (sdram_write != sdram_waiting_write ||
                              sdram_line != sdram_waiting_line ||
                              sdram_first != sdram_waiting_first))
            sdram_protocol_error("an SDRAM request changed before it was taken");

        // The word of the sdram_next in the cycle before
        if (sdram_word_due && sdram_busy && sdram_writes) begin
            if (^sdram_write_word === 1'bx)
                sdram_protocol_error("a word to write to SDRAM is X");
            sdram[{sdram_at_line, sdram_taken[2:0]}] = sdram_write_word;
            sdram_written[sdram_taken] = sdram_write_word;
            sdram_taken = sdram_taken + 1;
            if (sdram_taken == 8) begin
                if (sdram_log_fd != 0)
                    $fwrite(sdram_log_fd, "%0d write %06x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                            cycle, sdram_at_line, sdram_written[0], sdram_written[1],
                            sdram_written[2], sdram_written[3], sdram_written[4],
                            sdram_written[5], sdram_written[6], sdram_written[7]);
                sdram_busy    = 1'b0;
                sdram_idle_at = sdram_seen + sdram_n + 8 > cycle + 1
                              ? sdram_seen + sdram_n + 8 : cycle + 1;
            end
        end

        // A new request
        if (!sdram_busy && cycle >= sdram_idle_at && sdram_req) begin
            if (sdram_write && sdram_first != 3'd0)
                sdram_protocol_error("an SDRAM write doesn't start at word 0");
            sdram_take     <= 1'b1;
            sdram_busy     = 1'b1;
            sdram_writes   = sdram_write;
            sdram_at_line  = sdram_line;
            sdram_at_first = sdram_first;
            sdram_seen     = cycle;
            sdram_n        = sdram_request_latency(sdram_latency, $random(sdram_seed));
            sdram_sent     = 0;
            sdram_taken    = 0;
            if (sdram_write)
                sdram_wait = sdram_latency == SDRAM_RANDOM ? $unsigned($random(sdram_seed)) % 4 : 0;
            else
                sdram_wait = sdram_n - 1;
            if (!sdram_write && sdram_log_fd != 0)
                $fwrite(sdram_log_fd, "%0d read %06x %0d\n", cycle, sdram_line, sdram_first);
            sdram_waiting = 1'b0;
        end else begin
            sdram_waiting       = sdram_req && !sdram_take;
            sdram_waiting_write = sdram_write;
            sdram_waiting_line  = sdram_line;
            sdram_waiting_first = sdram_first;
        end

        // The next word, or the next sdram_next
        if (sdram_busy && sdram_sent < 8) begin
            if (sdram_wait == 0) begin
                if (sdram_writes) begin
                    sdram_next <= 1'b1;
                end else begin
                    sdram_read_valid <= 1'b1;
                    sdram_read_word  <= sdram[{sdram_at_line, sdram_at_first + sdram_sent[2:0]}];
                end
                sdram_sent = sdram_sent + 1;
                sdram_r    = $random(sdram_seed);
                sdram_wait = sdram_gap(sdram_latency, sdram_r);
                if (!sdram_writes && sdram_sent == 8) begin
                    sdram_busy    = 1'b0;
                    sdram_idle_at = cycle + 2;
                end
            end else begin
                sdram_wait = sdram_wait - 1;
            end
        end
    end
end

reg [8*8-1:0]   sdram_latency_arg;
reg [8*256-1:0] sdram_log_path;

initial begin
    if ($value$plusargs("sdram_latency=%s", sdram_latency_arg) && sdram_latency_arg == "rand")
        sdram_latency = SDRAM_RANDOM;
    else if (!$value$plusargs("sdram_latency=%d", sdram_latency))
        sdram_latency = 6;
    if (sdram_latency != SDRAM_RANDOM && sdram_latency < 1)
        $fatal(1, "+sdram_latency must be at least 1");
    if (!$value$plusargs("seed=%d", sdram_seed))
        sdram_seed = 0;
    if ($value$plusargs("sdram_log=%s", sdram_log_path))
        sdram_log_fd = $fopen(sdram_log_path, "w");
end
