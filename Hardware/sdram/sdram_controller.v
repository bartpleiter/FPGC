// SDRAM controller
// Two W9825G6KH-6 as one 32-bit SDRAM of 64 MiB, at 100 MHz, for the CPU's line port.
// Closed pages: every line is an ACTIVATE and a burst of 8 with auto-precharge.
module sdram_controller #(
    parameter PAUSE_CYCLES   = 20000,  // 200 us after a reset
    parameter REFRESH_CYCLES = 750     // 64 ms / 8,192 is 781
) (
    input  wire        clk,
    input  wire        reset,

    // Line port
    input  wire        sdram_req,
    input  wire        sdram_write,
    input  wire [25:5] sdram_line,
    input  wire [2:0]  sdram_first,
    output reg         sdram_take = 1'b0,
    output wire        sdram_read_valid,
    output reg  [31:0] sdram_read_word = 32'd0,
    output wire        sdram_next,
    input  wire [31:0] sdram_write_word,

    // SDRAM pins
    output wire        sdram_cs_n,
    output wire        sdram_ras_n,
    output wire        sdram_cas_n,
    output wire        sdram_we_n,
    output wire        sdram_cke,
    output reg  [12:0] sdram_a = 13'd0,
    output reg  [1:0]  sdram_ba = 2'd0,
    output reg  [3:0]  sdram_dqm = 4'b1111,
    inout  wire [31:0] sdram_dq
);

// Timing in cycles of 10 ns
localparam T_RCD = 2;  // ACTIVATE to READ or WRITE, 15 ns
localparam T_RP  = 2;  // precharge to ACTIVATE, 15 ns
localparam T_RC  = 6;  // AUTO REFRESH to the next command, 60 ns
localparam T_RSC = 2;  // MODE REGISTER SET to the next command, 2 cycles
localparam T_WR  = 2;  // last word written to precharge, 2 cycles
localparam BURST = 8;

// From a READ to its first word in sdram_read_word: CAS latency 2 and the clock's
// phase take 3 cycles to the input registers, and 1 more to sdram_read_word
localparam READ_DELAY = 4;

localparam [12:0] MODE = 13'h023;  // CAS latency 2, sequential bursts of 8

// {CS#, RAS#, CAS#, WE#}
localparam [3:0] NOP           = 4'b0111;
localparam [3:0] ACTIVATE      = 4'b0011;
localparam [3:0] READ          = 4'b0101;
localparam [3:0] WRITE         = 4'b0100;
localparam [3:0] PRECHARGE     = 4'b0010;
localparam [3:0] AUTO_REFRESH  = 4'b0001;
localparam [3:0] MODE_REGISTER = 4'b0000;

localparam [2:0] PAUSE        = 3'd0;
localparam [2:0] INIT_REFRESH = 3'd1;
localparam [2:0] INIT_MODE    = 3'd2;
localparam [2:0] IDLE         = 3'd3;
localparam [2:0] ROW          = 3'd4;

reg [2:0]  state = PAUSE;
reg [14:0] wait_count = PAUSE_CYCLES[14:0] - 15'd1;  // cycles until the next command
reg [3:0]  init_refreshes = 4'd0;
reg [9:0]  refresh_count = 10'd0;  // cycles until the next refresh is due

// The request
reg        writing = 1'b0;
reg [1:0]  bank = 2'd0;
reg [8:0]  column = 9'd0;

// Pin registers
reg [3:0]  command = NOP;
reg [31:0] dq_out = 32'd0;
reg        dq_oe = 1'b0;
reg [31:0] dq_in = 32'd0;

// Data schedules, one bit per cycle
reg [7:0]  asking = 8'd0;   // sdram_next
reg        word_ready = 1'b0;  // sdram_write_word has a word
reg [BURST+READ_DELAY-1:0] reading = 0;  // sdram_read_valid

assign {sdram_cs_n, sdram_ras_n, sdram_cas_n, sdram_we_n} = command;
assign sdram_cke = 1'b1;
assign sdram_dq = dq_oe ? dq_out : 32'bz;

assign sdram_next = asking[0];
assign sdram_read_valid = reading[0];

// Data
always @(posedge clk) begin
    dq_in           <= sdram_dq;
    sdram_read_word <= dq_in;
    word_ready      <= sdram_next && !reset;
    dq_out          <= sdram_write_word;
    dq_oe           <= word_ready && !reset;
end

// Commands
always @(posedge clk) begin
    command    <= NOP;
    sdram_a    <= 13'd0;
    sdram_ba   <= 2'd0;
    sdram_take <= 1'b0;
    asking     <= asking >> 1;
    reading    <= reading >> 1;

    if (wait_count != 15'd0)
        wait_count <= wait_count - 15'd1;
    if (refresh_count != 10'd0)
        refresh_count <= refresh_count - 10'd1;

    if (reset) begin
        state      <= PAUSE;
        wait_count <= PAUSE_CYCLES[14:0] - 15'd1;
        sdram_dqm  <= 4'b1111;
        asking     <= 8'd0;
        reading    <= 0;
    end else if (wait_count == 15'd0) begin
        case (state)
            PAUSE: begin
                command        <= PRECHARGE;
                sdram_a[10]    <= 1'b1;  // all banks
                wait_count     <= T_RP - 1;
                init_refreshes <= 4'd0;
                state          <= INIT_REFRESH;
            end

            INIT_REFRESH: begin
                command        <= AUTO_REFRESH;
                wait_count     <= T_RC - 1;
                refresh_count  <= REFRESH_CYCLES - 1;
                init_refreshes <= init_refreshes + 4'd1;
                if (init_refreshes == 4'd7)
                    state <= INIT_MODE;
            end

            INIT_MODE: begin
                command    <= MODE_REGISTER;
                sdram_a    <= MODE;
                sdram_dqm  <= 4'b0000;
                wait_count <= T_RSC - 1;
                state      <= IDLE;
            end

            IDLE: begin
                if (refresh_count == 10'd0) begin
                    command       <= AUTO_REFRESH;
                    wait_count    <= T_RC - 1;
                    refresh_count <= REFRESH_CYCLES - 1;
                end else if (sdram_req) begin
                    command    <= ACTIVATE;
                    sdram_ba   <= sdram_line[25:24];
                    sdram_a    <= sdram_line[23:11];
                    sdram_take <= 1'b1;
                    writing    <= sdram_write;
                    bank       <= sdram_line[25:24];
                    column     <= {sdram_line[10:5], sdram_first};
                    // A word is on the pins 2 cycles after its sdram_next, with the
                    // WRITE tRCD after the ACTIVATE
                    if (sdram_write)
                        asking <= 8'hFF;
                    wait_count <= T_RCD - 1;
                    state      <= ROW;
                end
            end

            ROW: begin
                command  <= writing ? WRITE : READ;
                sdram_ba <= bank;
                sdram_a  <= {2'b00, 1'b1, 1'b0, column};  // A10: auto-precharge
                if (writing)
                    wait_count <= BURST - 1 + T_WR + T_RP - 1;
                else begin
                    wait_count <= BURST + T_RP - 1;
                    reading    <= {{BURST{1'b1}}, {READ_DELAY{1'b0}}};
                end
                state <= IDLE;
            end

            default:
                state <= PAUSE;
        endcase
    end
end

endmodule
