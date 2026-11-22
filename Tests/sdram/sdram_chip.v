// A model of one W9825G6KH-6 SDRAM chip, for simulation only: 4 banks of 8,192 rows of
// 512 columns of 16 bits, 32 MiB. Written from the datasheet (Rev A04, March 2017), for
// the -6 grade, and independent of the controller: its timing checks are in ps of
// simulation time, the controller's timing is in cycles.
//
// It has the commands the controller uses: ACTIVATE, READ and WRITE with and without
// auto-precharge, PRECHARGE (one bank or all), AUTO REFRESH, MODE REGISTER SET, NOP and
// DESELECT. It has one mode: CAS latency 2, sequential bursts of 8 (mode register
// 0x023). It has no interrupted or seamless bursts, no BURST STOP, no power down and no
// self refresh.
//
// At every rising edge of `clk`, it checks the rules below. The first rule that's broken
// sets `error` to its number and prints a line "sdram error <n> ..." with the time and
// the instance; the testbench stops on it. The numbers are also in test_sdram_chip.py.
//
//    1  a pin is X or Z: CKE, CS#, DQM, or RAS#, CAS#, WE#, the bank and the address
//       of a command
//    2  CKE is low
//    3  the pause after power up: a command before PAUSE, or DQM not high before the
//       first command
//    4  the initialization: the first command isn't PRECHARGE ALL, or an ACTIVATE
//       comes before the MODE REGISTER SET and 8 AUTO REFRESH
//    5  a MODE REGISTER SET with another mode than 0x023
//    6  BURST STOP
//    7  an ACTIVATE of an active bank, or an AUTO REFRESH or MODE REGISTER SET while a
//       bank is active
//    8  a READ or WRITE to a bank that isn't active
//    9  tRCD: a READ or WRITE less than 15 ns after the bank's ACTIVATE
//   10  tRP: an ACTIVATE, AUTO REFRESH or MODE REGISTER SET less than 15 ns after a
//       precharge started
//   11  tRAS: a precharge (also an auto-precharge) less than 42 ns after the ACTIVATE
//   12  tRC: an ACTIVATE less than 60 ns after the bank's last ACTIVATE, or any command
//       less than 60 ns after an AUTO REFRESH
//   13  tRRD: two ACTIVATEs less than 2 edges apart
//   14  tRSC: any command less than 2 edges after a MODE REGISTER SET
//   15  tWR: a PRECHARGE less than 2 edges after the last word of a write
//   16  a READ, WRITE or PRECHARGE during a burst, or to a bank in an auto-precharge
//   17  the refresh: more than 7.8125 us (64 ms / 8,192) between two AUTO REFRESH
//       commands, from the end of the initialization on
//   18  a word to write is X or Z in a byte that DQM doesn't mask
//   19  something else drives DQ while the chip drives a word
//
// Read data, for a READ at edge n: the chip drives DQ from edge n + 1 on (tLZ 0), each
// word k from tAC (6 ns) after edge n + 1 + k until tOH (3 ns) after the edge after it,
// and X in between. After the last word, it's X until tHZ (6 ns), then Z. So a word can
// only be captured in the window of its own edge. DQM masks a word of a read 2 edges
// after it's sampled, and a word of a write at the same edge.
//
// The memory is 2-state and starts at 0. A reset of the controller pauses the refresh,
// which a testbench declares with the task pause_refresh_check: the check of rule 17
// starts again at the next AUTO REFRESH.
`timescale 1ps / 1ps
module sdram_chip #(
    parameter PAUSE = 200_000_000  // ps, the pause after power up (200 us)
) (
    input  wire        clk,
    input  wire        cke,
    input  wire        cs_n,
    input  wire        ras_n,
    input  wire        cas_n,
    input  wire        we_n,
    input  wire [1:0]  ba,
    input  wire [12:0] a,
    input  wire [1:0]  dqm,
    inout  wire [15:0] dq,
    output reg  [4:0]  error = 5'd0
);

// The datasheet's timing, in ps
localparam T_RCD   = 15_000;
localparam T_RP    = 15_000;
localparam T_RAS   = 42_000;
localparam T_RC    = 60_000;
localparam T_AC    = 6_000;
localparam T_OH    = 3_000;
localparam T_HZ    = 6_000;
localparam REFRESH = 7_812_500;

// The datasheet's timing in clock edges
localparam RRD   = 2;
localparam WR    = 2;
localparam RSC   = 2;
localparam BURST = 8;

localparam [14:0] MODE = 15'h0023;  // {bank, address}: CAS latency 2, sequential, 8

localparam ERR_X        = 1;
localparam ERR_CKE      = 2;
localparam ERR_PAUSE    = 3;
localparam ERR_INIT     = 4;
localparam ERR_MODE     = 5;
localparam ERR_COMMAND  = 6;
localparam ERR_ACTIVE   = 7;
localparam ERR_IDLE     = 8;
localparam ERR_TRCD     = 9;
localparam ERR_TRP      = 10;
localparam ERR_TRAS     = 11;
localparam ERR_TRC      = 12;
localparam ERR_TRRD     = 13;
localparam ERR_TRSC     = 14;
localparam ERR_TWR      = 15;
localparam ERR_BURST    = 16;
localparam ERR_REFRESH  = 17;
localparam ERR_WRITE_X  = 18;
localparam ERR_CONFLICT = 19;

bit [15:0] memory [0:(1 << 24) - 1];  // {bank, row, column}

// The initialization
reg     started = 1'b0;    // a command other than NOP has come
reg     mode_set = 1'b0;
integer refreshes = 0;     // AUTO REFRESH commands so far
reg     refresh_checked = 1'b0;
time    refresh_at = 0;    // the last AUTO REFRESH
integer mode_edge = -100;  // the edge of the last MODE REGISTER SET
integer act_edge = -100;   // the edge of the last ACTIVATE

// The banks
reg        active [0:3];
reg [12:0] row [0:3];
time       act_at [0:3];      // the last ACTIVATE
time       ready_at [0:3];    // the end of the last precharge's tRP
integer    ap_edge [0:3];     // the edge at which an auto-precharge starts, or -1
integer    write_edge [0:3];  // the edge of the last word written

// The burst
reg        reading = 1'b0;
reg        writing = 1'b0;
integer    burst_edge;        // the edge of the READ or WRITE
reg [1:0]  burst_bank;
reg [12:0] burst_row;
reg [8:0]  burst_column;

// The read data
reg [15:0] drive = 16'bz;
reg        showing = 1'b0;    // drives a word, for rule 19
reg [1:0]  dqm_before = 2'b11;

assign dq = drive;

integer edge_n = 0;
time    now;
integer b;
integer k;

initial
    for (b = 0; b < 4; b = b + 1) begin
        active[b]     = 1'b0;
        row[b]        = 13'd0;
        act_at[b]     = 0;
        ready_at[b]   = 0;
        ap_edge[b]    = -1;
        write_edge[b] = -100;
    end

function [8*44-1:0] rule(input integer number);
    case (number)
        ERR_X:        rule = "a pin is X or Z";
        ERR_CKE:      rule = "CKE is low";
        ERR_PAUSE:    rule = "the pause after power up";
        ERR_INIT:     rule = "the initialization's order";
        ERR_MODE:     rule = "a mode other than 0x023";
        ERR_COMMAND:  rule = "BURST STOP";
        ERR_ACTIVE:   rule = "a bank is active";
        ERR_IDLE:     rule = "the bank isn't active";
        ERR_TRCD:     rule = "tRCD";
        ERR_TRP:      rule = "tRP";
        ERR_TRAS:     rule = "tRAS";
        ERR_TRC:      rule = "tRC";
        ERR_TRRD:     rule = "tRRD";
        ERR_TRSC:     rule = "tRSC";
        ERR_TWR:      rule = "tWR";
        ERR_BURST:    rule = "a command during a burst or auto-precharge";
        ERR_REFRESH:  rule = "no AUTO REFRESH for 7.8125 us";
        ERR_WRITE_X:  rule = "a word to write is X or Z";
        ERR_CONFLICT: rule = "something else drives DQ";
        default:      rule = "?";
    endcase
endfunction

task fail(input integer number);
    if (error == 5'd0) begin
        error = number[4:0];
        $display("sdram error %0d at %0t ps in %m: %0s", number, $time, rule(number));
    end
endtask

task pause_refresh_check;
    refresh_checked = 1'b0;
endtask

// A precharge of bank `bank` starts at this edge
task precharge(input integer bank);
    begin
        if (now - act_at[bank] < T_RAS)
            fail(ERR_TRAS);
        if (edge_n - write_edge[bank] < WR)
            fail(ERR_TWR);
        active[bank]   = 1'b0;
        ready_at[bank] = now + T_RP;
        ap_edge[bank]  = -1;
    end
endtask

// For an AUTO REFRESH and a MODE REGISTER SET: every bank is precharged
task check_banks_idle;
    if (active[0] || active[1] || active[2] || active[3])
        fail(ERR_ACTIVE);
    else if (now < ready_at[0] || now < ready_at[1] || now < ready_at[2] ||
             now < ready_at[3])
        fail(ERR_TRP);
endtask

// The memory address of word k of the burst
function [23:0] burst_word(input integer k);
    burst_word = {burst_bank, burst_row, burst_column[8:3], burst_column[2:0] + k[2:0]};
endfunction

// A burst runs, or bank `bank` waits for its auto-precharge
function busy(input integer bank);
    busy = (reading && edge_n <= burst_edge + BURST) ||
           (writing && edge_n < burst_edge + BURST) ||
           ap_edge[bank] > edge_n;
endfunction

always @(posedge clk) begin
    now = $time;

    // The pins
    if (^{cke, cs_n, dqm} === 1'bx || (cs_n === 1'b0 && ^{ras_n, cas_n, we_n} === 1'bx))
        fail(ERR_X);
    else if (!cke)
        fail(ERR_CKE);
    if (!started && dqm !== 2'b11)
        fail(ERR_PAUSE);

    // The auto-precharges that start at this edge
    for (b = 0; b < 4; b = b + 1)
        if (ap_edge[b] == edge_n)
            precharge(b);

    // The read data
    if (reading) begin
        k = edge_n - burst_edge - 1;
        if (k >= 0 && k < BURST) begin
            showing <= #T_OH 1'b0;
            drive   <= #T_OH 16'bx;
            drive   <= #T_AC {dqm_before[1] ? 8'bz : memory[burst_word(k)][15:8],
                              dqm_before[0] ? 8'bz : memory[burst_word(k)][7:0]};
            showing <= #T_AC dqm_before == 2'b00;
        end else if (k == BURST) begin
            showing <= #T_OH 1'b0;
            drive   <= #T_OH 16'bx;
            drive   <= #T_HZ 16'bz;
            reading = 1'b0;
        end
    end

    // The command
    if (cs_n === 1'b0 && {ras_n, cas_n, we_n} !== 3'b111) begin
        if (!started) begin
            started = 1'b1;
            if (now < PAUSE)
                fail(ERR_PAUSE);
            else if ({ras_n, cas_n, we_n} != 3'b010 || !a[10])
                fail(ERR_INIT);
        end
        if ({ras_n, cas_n, we_n} != 3'b001 && ^{ba, a} === 1'bx)
            fail(ERR_X);
        if (refreshes > 0 && now - refresh_at < T_RC)
            fail(ERR_TRC);
        if (edge_n - mode_edge < RSC)
            fail(ERR_TRSC);

        case ({ras_n, cas_n, we_n})
            3'b011: begin  // ACTIVATE
                if (!mode_set || refreshes < 8)
                    fail(ERR_INIT);
                else if (active[ba])
                    fail(ERR_ACTIVE);
                else if (now < ready_at[ba])
                    fail(ERR_TRP);
                else if (now - act_at[ba] < T_RC)
                    fail(ERR_TRC);
                else if (edge_n - act_edge < RRD)
                    fail(ERR_TRRD);
                active[ba]     = 1'b1;
                row[ba]        = a;
                act_at[ba]     = now;
                act_edge       = edge_n;
                write_edge[ba] = -100;
            end
            3'b101, 3'b100: begin  // READ, WRITE
                if (!active[ba])
                    fail(ERR_IDLE);
                else if (now - act_at[ba] < T_RCD)
                    fail(ERR_TRCD);
                else if (busy(ba))
                    fail(ERR_BURST);
                reading      = we_n;
                writing      = !we_n;
                burst_edge   = edge_n;
                burst_bank   = ba;
                burst_row    = row[ba];
                burst_column = a[8:0];
                if (a[10])
                    ap_edge[ba] = we_n ? edge_n + BURST : edge_n + BURST - 1 + WR;
            end
            3'b010: begin  // PRECHARGE
                for (b = 0; b < 4; b = b + 1)
                    if (a[10] || ba == b) begin
                        if (busy(b))
                            fail(ERR_BURST);
                        else if (active[b])
                            precharge(b);
                    end
            end
            3'b001: begin  // AUTO REFRESH
                check_banks_idle;
                refreshes  = refreshes + 1;
                refresh_at = now;
                if (mode_set && refreshes >= 8)
                    refresh_checked = 1'b1;
            end
            3'b000: begin  // MODE REGISTER SET
                check_banks_idle;
                if ({ba, a} !== MODE)
                    fail(ERR_MODE);
                mode_set  = 1'b1;
                mode_edge = edge_n;
                if (refreshes >= 8)
                    refresh_checked = 1'b1;
            end
            default:  // BURST STOP
                fail(ERR_COMMAND);
        endcase
    end

    // The write data
    if (writing) begin
        k = edge_n - burst_edge;
        if (k >= 0 && k < BURST) begin
            if ((!dqm[1] && ^dq[15:8] === 1'bx) || (!dqm[0] && ^dq[7:0] === 1'bx))
                fail(ERR_WRITE_X);
            if (!dqm[1])
                memory[burst_word(k)][15:8] = dq[15:8];
            if (!dqm[0])
                memory[burst_word(k)][7:0] = dq[7:0];
            write_edge[burst_bank] = edge_n;
            if (k == BURST - 1)
                writing = 1'b0;
        end
    end

    if (refresh_checked && now - refresh_at > REFRESH)
        fail(ERR_REFRESH);

    dqm_before = dqm;
    edge_n = edge_n + 1;
end

always @(dq)
    if (showing && dq !== drive)
        fail(ERR_CONFLICT);

endmodule
