// A model of the W25Q128JV SPI flash, for simulation only. Written from the datasheet
// (Rev F, March 2018), for the -IQ parts: the Quad Enable bit is fixed to 1, so IO2 and
// IO3 are data lines, and WP and HOLD don't exist.
//
// It has the commands the FPGC uses: JEDEC ID (9Fh), Read Status Register 1 (05h),
// Write Enable (06h), Write Disable (04h), Read Data (03h), Fast Read (0Bh), Fast Read
// Quad Output (6Bh), Page Program (02h), Sector Erase (20h) and Block Erase 64 KiB
// (D8h). Commands come in on IO0 at the rising edge of the clock, and data goes out at
// the falling edge: on IO1, or on IO0 to IO3 for 6Bh, the high nibble first. Program
// and erase take PROGRAM_TIME and ERASE_TIME instead of milliseconds.
//
// It checks the rules below. The first rule that's broken sets `error` to its number
// and prints a line "flash error <n> ..." with the time and the instance; the testbench
// stops on it. The numbers are also in test_flash_chip.py.
//
//    1  a pin is X or Z: CS#, the clock while selected, or IO0 at a rising edge while
//       the chip takes a command, an address or data
//    2  tSHSL: CS# high for less than 10 ns, or less than 50 ns after a Write Enable,
//       Write Disable, program or erase
//    3  tSLCH, tCHSH, tSHCH: a rising edge of the clock less than 3 ns from an edge of
//       CS#
//    4  the clock is high or low for less than 9 ns during a Read Data (50 MHz), or
//       less than 3.4 ns during the other commands (133 MHz)
//    5  tDVCH, tCHDX: IO0 changes less than 1 ns before or 2 ns after a rising edge,
//       while the chip takes input
//    6  an unknown command
//    7  a command other than 05h while a program or erase is busy
//    8  a program or erase without Write Enable
//    9  CS# rises in the middle of a byte of a Page Program, or an erase doesn't have
//       exactly 3 address bytes (the chip ignores the command; here it's a mistake)
//   10  a Page Program goes past the end of its page (the chip wraps around)
//   11  something else drives a line that the chip drives
//
// After a falling edge, a line the chip drives is X from tCLQX (1.5 ns) and has its bit
// from tCLQV (6 ns). After CS# rises, the lines are Z from tSHQZ (7 ns).
//
// The memory starts erased (all 1s). INIT_FILE, if given, is read into it from address
// 0 with $readmemh.
`timescale 1ps / 1ps
module flash_chip #(
    parameter ADDRESS_BITS = 24,          // 16 MiB; the address wraps around above it
    parameter INIT_FILE    = "",
    parameter PROGRAM_TIME = 2_000_000,   // ps
    parameter ERASE_TIME   = 5_000_000    // ps
) (
    input  wire       cs_n,
    input  wire       clk,
    inout  wire [3:0] io,
    output reg  [3:0] error = 4'd0
);

// The datasheet's timing, in ps
localparam T_SHSL1 = 10_000;
localparam T_SHSL2 = 50_000;
localparam T_CS    = 3_000;   // tSLCH, tCHSH, tSHCH
localparam T_CLH_R = 9_000;   // 45 % of 20 ns, for Read Data
localparam T_CLH   = 3_400;   // 45 % of 7.5 ns, for the rest
localparam T_DVCH  = 1_000;
localparam T_CHDX  = 2_000;
localparam T_CLQX  = 1_500;
localparam T_CLQV  = 6_000;
localparam T_SHQZ  = 7_000;

localparam ERR_X        = 1;
localparam ERR_TSHSL    = 2;
localparam ERR_TCS      = 3;
localparam ERR_CLOCK    = 4;
localparam ERR_SETUP    = 5;
localparam ERR_COMMAND  = 6;
localparam ERR_BUSY     = 7;
localparam ERR_WEL      = 8;
localparam ERR_ABORT    = 9;
localparam ERR_WRAP     = 10;
localparam ERR_CONFLICT = 11;

localparam SIZE = 1 << ADDRESS_BITS;

bit [7:0] memory [0:SIZE-1];

// The status
reg busy = 1'b0;
reg wel  = 1'b0;

// The command
reg        selected = 1'b0;
integer    n = 0;              // rising edges since CS# fell
reg [31:0] shift = 32'd0;      // the bits taken, the last in bit 0
reg [7:0]  opcode = 8'd0;
reg [23:0] address = 24'd0;
reg [7:0]  page [0:255];       // the data of a Page Program
integer    page_bytes = 0;
reg        writes = 1'b0;      // the last command was a write enable, program or erase

// The output
reg [3:0] drive = 4'b0000;     // the lines the chip drives
reg [3:0] value = 4'bxxxx;
reg [3:0] shown = 4'bxxxx;     // the value after tCLQV, for rule 11
reg [3:0] next_drive;
reg [3:0] next_value;

assign io[0] = drive[0] ? value[0] : 1'bz;
assign io[1] = drive[1] ? value[1] : 1'bz;
assign io[2] = drive[2] ? value[2] : 1'bz;
assign io[3] = drive[3] ? value[3] : 1'bz;

// Times of the last events
time cs_fell = 0;
time cs_rose = 0;
time rose_at = 0;
time fell_at = 0;
time io0_at = 0;

integer i;

initial begin
    for (i = 0; i < SIZE; i = i + 1)
        memory[i] = 8'hFF;
    if (INIT_FILE != "")
        $readmemh(INIT_FILE, memory);
end

function [8*48-1:0] rule(input integer number);
    case (number)
        ERR_X:        rule = "a pin is X or Z";
        ERR_TSHSL:    rule = "tSHSL";
        ERR_TCS:      rule = "a clock edge too close to CS#";
        ERR_CLOCK:    rule = "the clock is too fast";
        ERR_SETUP:    rule = "IO0's setup or hold";
        ERR_COMMAND:  rule = "an unknown command";
        ERR_BUSY:     rule = "a command while busy";
        ERR_WEL:      rule = "a program or erase without Write Enable";
        ERR_ABORT:    rule = "a program or erase that the chip ignores";
        ERR_WRAP:     rule = "a Page Program past the end of its page";
        ERR_CONFLICT: rule = "something else drives a line the chip drives";
        default:      rule = "?";
    endcase
endfunction

task fail(input integer number);
    if (error == 4'd0) begin
        error = number[3:0];
        $display("flash error %0d at %0t ps in %m: %0s", number, $time, rule(number));
    end
endtask

// The chip takes input on IO0 at rising edge n + 1
function taking(input integer edges);
    case (opcode)
        8'h03, 8'h0B, 8'h6B, 8'h20, 8'hD8: taking = edges < 32;
        8'h02:                             taking = 1'b1;
        default:                           taking = edges < 8;
    endcase
endfunction

// The bits the chip drives after the falling edge that follows rising edge n
task output_bits;
    integer j;
    reg [7:0] b;
    reg [23:0] id;
    reg [7:0] status;
    begin
        next_drive = 4'b0000;
        next_value = 4'bxxxx;
        id         = 24'hEF4018;
        status     = {6'd0, wel, busy};
        if (n >= 8)
            case (opcode)
                8'h9F: begin
                    next_drive[1] = 1'b1;
                    next_value[1] = n < 32 ? id[31 - n] : 1'bx;
                end
                8'h05: begin
                    next_drive[1] = 1'b1;
                    next_value[1] = status[7 - n % 8];
                end
                8'h03, 8'h0B: if (n >= (opcode == 8'h03 ? 32 : 40)) begin
                    j = n - (opcode == 8'h03 ? 32 : 40);
                    b = memory[(address + j / 8) % SIZE];
                    next_drive[1] = 1'b1;
                    next_value[1] = b[7 - j % 8];
                end
                8'h6B: if (n >= 40) begin
                    j = n - 40;
                    b = memory[(address + j / 2) % SIZE];
                    next_drive = 4'b1111;
                    next_value = j % 2 == 0 ? b[7:4] : b[3:0];
                end
                default: ;
            endcase
    end
endtask

// CS#
always @(cs_n) begin
    if (cs_n !== 1'b0 && cs_n !== 1'b1) begin
        fail(ERR_X);
    end else if (!cs_n) begin
        if ($time - cs_rose < (writes ? T_SHSL2 : T_SHSL1) && cs_rose != 0)
            fail(ERR_TSHSL);
        cs_fell  = $time;
        selected = 1'b1;
        n        = 0;
        writes   = 1'b0;
    end else begin
        if ($time - rose_at < T_CS)
            fail(ERR_TCS);
        cs_rose  = $time;
        selected = 1'b0;
        finish_command;
        drive <= #T_SHQZ 4'b0000;
    end
end

// The command is complete when CS# rises
task finish_command;
    integer k;
    begin
        if (n >= 8)
            case (opcode)
                8'h06: begin
                    wel    = 1'b1;
                    writes = 1'b1;
                end
                8'h04: begin
                    wel    = 1'b0;
                    writes = 1'b1;
                end
                8'h02: begin
                    writes = 1'b1;
                    if (n < 40 || n % 8 != 0) begin
                        fail(ERR_ABORT);
                    end else if (!wel) begin
                        fail(ERR_WEL);
                    end else begin
                        for (k = 0; k < page_bytes; k = k + 1)
                            memory[({address[23:8], 8'd0} + (address[7:0] + k) % 256) % SIZE]
                                &= page[k];
                        start_busy(PROGRAM_TIME);
                    end
                end
                8'h20, 8'hD8: begin
                    writes = 1'b1;
                    if (n != 32) begin
                        fail(ERR_ABORT);
                    end else if (!wel) begin
                        fail(ERR_WEL);
                    end else begin
                        for (k = 0; k < (opcode == 8'h20 ? 4096 : 65536); k = k + 1)
                            memory[((opcode == 8'h20 ? {address[23:12], 12'd0}
                                                     : {address[23:16], 16'd0}) + k) % SIZE]
                                = 8'hFF;
                        start_busy(ERASE_TIME);
                    end
                end
                default: ;
            endcase
    end
endtask

task start_busy(input time duration);
    begin
        busy = 1'b1;
        busy <= #duration 1'b0;
        wel  <= #duration 1'b0;
    end
endtask

// At a rising edge, the chip takes a bit
always @(posedge clk) if (selected) begin
    if ($time - cs_fell < T_CS)
        fail(ERR_TCS);
    if (n > 0 && $time - fell_at < (opcode == 8'h03 && n >= 8 ? T_CLH_R : T_CLH))
        fail(ERR_CLOCK);
    if (taking(n)) begin
        if (io[0] !== 1'b0 && io[0] !== 1'b1)
            fail(ERR_X);
        if ($time - io0_at < T_DVCH)
            fail(ERR_SETUP);
    end
    for (i = 0; i < 4; i = i + 1)
        if (drive[i] && shown[i] !== 1'bx && io[i] !== shown[i])
            fail(ERR_CONFLICT);
    rose_at = $time;
    shift   = {shift[30:0], io[0]};
    n       = n + 1;

    if (n == 8) begin
        opcode = shift[7:0];
        if (busy && opcode != 8'h05)
            fail(ERR_BUSY);
        case (opcode)
            8'h9F, 8'h05, 8'h06, 8'h04, 8'h03, 8'h0B, 8'h6B, 8'h02, 8'h20, 8'hD8: ;
            default: fail(ERR_COMMAND);
        endcase
        page_bytes = 0;
    end
    if (n == 32)
        address = shift[23:0];
    if (opcode == 8'h02 && n > 32 && n % 8 == 0) begin
        if (page_bytes == 256 || address[7:0] + page_bytes == 256)
            fail(ERR_WRAP);
        if (page_bytes < 256) begin
            page[page_bytes] = shift[7:0];
            page_bytes = page_bytes + 1;
        end
    end
end

// At a falling edge, the chip shows its next bits
always @(negedge clk) if (selected) begin
    if ($time - rose_at < (opcode == 8'h03 && n > 8 ? T_CLH_R : T_CLH) && n > 0)
        fail(ERR_CLOCK);
    fell_at = $time;
    output_bits;
    drive = next_drive;
    shown = 4'bxxxx;
    value <= #T_CLQX 4'bxxxx;
    value <= #T_CLQV next_value;
    shown <= #T_CLQV next_value;
end

// The clock, and IO0's hold
always @(clk)
    if (selected && clk !== 1'b0 && clk !== 1'b1)
        fail(ERR_X);

always @(io[0]) begin
    if (selected && clk === 1'b1 && $time - rose_at < T_CHDX && taking(n - 1))
        fail(ERR_SETUP);
    io0_at = $time;
end

endmodule
