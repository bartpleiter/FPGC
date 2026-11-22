// The board's SDRAM in a testbench: the SDRAM controller on the CPU's SDRAM line port,
// and two chip models (sdram_chip.v) as the 32-bit SDRAM, as in tb_sdram_controller.v.
// Include it inside a testbench module, before the instance that connects to the port,
// instead of sdram_line_model.vh. It declares the port's signals, sdram_req to
// sdram_write_word. The testbench must have `clk`, `reset`, `cycle` (an integer, the
// number of the current cycle) and a task `stop` that ends the simulation.
//
// The chips get the clock 7.9 ns later (sdram decision 6), and the pause after power up
// and after a reset is 1 us (100 cycles) instead of 200 us. A reset of the testbench
// resets the controller too, as on the board, and pauses the chips' refresh check
// (sdram decision 11). When a chip reports a broken rule, the run stops with a line
// "sdram error: ...".
//
// For the testbench, the SDRAM's words: the function memory_word and the task
// set_memory_word read and write the word at a word address (byte address [25:2]),
// which is also the chips' {bank, row, column}.
//
// Plusargs:
//
//   +sdram_log=<file>  writes a line for each request, as sdram_line_model.vh does:
//                      "<cycle> read <line> <first>" when a read is taken, and
//                      "<cycle> write <line> <8 words>" when a write has given its last
//                      word, with the line as bits [25:5] of the address, in hex

// ---- The SDRAM line port ----

wire        sdram_req;
wire        sdram_write;
wire [25:5] sdram_line;
wire [2:0]  sdram_first;
wire        sdram_take;
wire        sdram_read_valid;
wire [31:0] sdram_read_word;
wire        sdram_next;
wire [31:0] sdram_write_word;

// ---- The controller and the chips ----

localparam SDRAM_PAUSE_CYCLES = 100;

reg sdram_clk = 1'b0;
always @(clk)
    sdram_clk <= #7.9 clk;

wire        sdram_cs_n;
wire        sdram_ras_n;
wire        sdram_cas_n;
wire        sdram_we_n;
wire        sdram_cke;
wire [12:0] sdram_a;
wire [1:0]  sdram_ba;
wire [3:0]  sdram_dqm;
wire [31:0] sdram_dq;
wire [4:0]  sdram_error_lo;
wire [4:0]  sdram_error_hi;

sdram_controller #(
    .PAUSE_CYCLES(SDRAM_PAUSE_CYCLES)
) sdram (
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
    .sdram_cs_n(sdram_cs_n),
    .sdram_ras_n(sdram_ras_n),
    .sdram_cas_n(sdram_cas_n),
    .sdram_we_n(sdram_we_n),
    .sdram_cke(sdram_cke),
    .sdram_a(sdram_a),
    .sdram_ba(sdram_ba),
    .sdram_dqm(sdram_dqm),
    .sdram_dq(sdram_dq)
);

sdram_chip #(
    .PAUSE(SDRAM_PAUSE_CYCLES * 10_000)
) sdram_lo (
    .clk(sdram_clk),
    .cke(sdram_cke),
    .cs_n(sdram_cs_n),
    .ras_n(sdram_ras_n),
    .cas_n(sdram_cas_n),
    .we_n(sdram_we_n),
    .ba(sdram_ba),
    .a(sdram_a),
    .dqm(sdram_dqm[1:0]),
    .dq(sdram_dq[15:0]),
    .error(sdram_error_lo)
);

sdram_chip #(
    .PAUSE(SDRAM_PAUSE_CYCLES * 10_000)
) sdram_hi (
    .clk(sdram_clk),
    .cke(sdram_cke),
    .cs_n(sdram_cs_n),
    .ras_n(sdram_ras_n),
    .cas_n(sdram_cas_n),
    .we_n(sdram_we_n),
    .ba(sdram_ba),
    .a(sdram_a),
    .dqm(sdram_dqm[3:2]),
    .dq(sdram_dq[31:16]),
    .error(sdram_error_hi)
);

function [31:0] memory_word(input [23:0] address);
    memory_word = {sdram_hi.memory[address], sdram_lo.memory[address]};
endfunction

task set_memory_word(input [23:0] address, input [31:0] word);
    begin
        sdram_hi.memory[address] = word[31:16];
        sdram_lo.memory[address] = word[15:0];
    end
endtask

always @(posedge clk) begin
    if (reset) begin
        sdram_lo.pause_refresh_check;
        sdram_hi.pause_refresh_check;
    end
    if (sdram_error_lo != 5'd0 || sdram_error_hi != 5'd0) begin
        $display("sdram error: a chip model reports a broken rule in cycle %0d", cycle);
        stop;
    end
end

// ---- The log ----

integer     sdram_log_fd = 0;
reg [8*256-1:0] sdram_log_path;
reg [25:5]  sdram_log_line;
reg [31:0]  sdram_log_words [0:7];
integer     sdram_log_count = 8;  // the words of the write so far
reg         sdram_log_due = 1'b0;  // sdram_next was 1 in the cycle before

initial
    if ($value$plusargs("sdram_log=%s", sdram_log_path))
        sdram_log_fd = $fopen(sdram_log_path, "w");

always @(posedge clk) begin
    sdram_log_due <= sdram_next;
    if (reset) begin
        sdram_log_count = 8;
    end else begin
        if (sdram_log_due && sdram_log_count < 8) begin
            sdram_log_words[sdram_log_count] = sdram_write_word;
            sdram_log_count = sdram_log_count + 1;
            if (sdram_log_count == 8 && sdram_log_fd != 0)
                $fwrite(sdram_log_fd, "%0d write %06x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                        cycle, sdram_log_line, sdram_log_words[0], sdram_log_words[1],
                        sdram_log_words[2], sdram_log_words[3], sdram_log_words[4],
                        sdram_log_words[5], sdram_log_words[6], sdram_log_words[7]);
        end
        if (sdram_take) begin
            if (sdram_write) begin
                sdram_log_line  = sdram_line;
                sdram_log_count = 0;
            end else if (sdram_log_fd != 0) begin
                $fwrite(sdram_log_fd, "%0d read %06x %0d\n", cycle, sdram_line, sdram_first);
            end
        end
    end
end
