/*
* SDRAM controller
* Custom made for FPGC with l1 cache (8 32-bit words per cache line), having two W9825G6KH-6 chips
* Should run at 100MHz (with the SDRAM_CLK outside of this module at 180 degrees phase shifted)
* Uses a 256 bit bus to the CPU/Cache controller, similar to a MIG 7
* In contrast to MIG 7, the address is given in 256 bit words, not in bytes
*
* Some calculations:
* - SDRAMx2 size: 64MiB (16M x 32 x 4 banks) -> 16 MiWords -> 2 MiLines (256 bits per line)
* - Addressing: log2(2097152) = 21 bits 
*/
module SDRAMcontroller(
    // Clock and reset
    input wire          clk,
    input wire          reset,

    // CPU/Cache controller interface
    input wire [20:0]   cpu_addr,
    input wire [255:0]  cpu_data,
    input wire          cpu_we,
    input wire          cpu_start,
    output reg          cpu_done = 1'b0,
    output reg [255:0]  cpu_q = 256'd0,

    // SDRAM signals
    output wire         SDRAM_CSn,
    output wire         SDRAM_WEn,
    output wire         SDRAM_CASn,
    output wire         SDRAM_RASn,
    output wire         SDRAM_CKE,
    output reg [12:0]   SDRAM_A   = 13'd0,
    output reg [1:0]    SDRAM_BA  = 2'd0,
    output reg [3:0]    SDRAM_DQM = 4'b0000,
    inout [31:0]        SDRAM_DQ
);

//==============Refresh Settings=================
//   100MHz -> 100.000.000 cycles per sec
//   100.000.000*0,064 -> 6.400.000 cycles per 64ms
//   6.400.000 / 8192 auto refreshes -> refresh after ~782 cycles
localparam cycles_per_refresh = 782;
// Note: after adjusting make sure that cycles_per_refresh fits in the refresh_counter width
reg [9:0] refresh_counter = 10'd0;

//==============Init Settings=================
`ifndef __ICARUS__
localparam sdram_startup_cycles = 20000;  // 200us @ 100MHz -> 20.000 cycles
`else
localparam sdram_startup_cycles = 10;  // Lowered for simulation
`endif

reg [15:0] startup_counter = 16'd0;     // Cycle counter for startup

//==============Mode Register=================
// Mode register value
// {3'b reserved, 1'b write mode, 1'b reserved, 1'b test mode,
//  3'b CAS latency, 1'b addressing mode, 3'b burst length}
// CAS latency: 010=2, 011=3
// addressing mode: 0=seq 1=interleave
// burst length: 000=1, 001=2, 010=4, 011=8, 111=full page
// We need: CAS latency 2, sequential, burst length 8
localparam MODE_REG = {3'b0, 1'b0, 1'b0, 1'b0, 3'b010, 1'b0, 3'b011};

//==============Ports=================
// DQ Port
// Write
reg [31:0] SDRAM_DATA = 32'd0;
reg SDRAM_DQ_OE = 1'b0;
assign SDRAM_DQ = SDRAM_DQ_OE ? SDRAM_DATA : 32'hZZZZ;
// Read
wire [31:0] SDRAM_Q;
assign SDRAM_Q = SDRAM_DQ;

localparam PRECHARGE_ADDR_IDX = 10; // Address bit of precharge all banks
reg [3:0] SDRAM_CMD = CMD_NOP;
assign {SDRAM_CSn, SDRAM_RASn, SDRAM_CASn, SDRAM_WEn} = SDRAM_CMD;
assign SDRAM_CKE = 1'b1; // No reason to disable clock enable

//==============SDRAM Commands=================
localparam
    CMD_UNSELECTED          = 4'b1000,
    CMD_NOP                 = 4'b0111,
    CMD_READ                = 4'b0101,
    CMD_WRITE               = 4'b0100,
    CMD_BANK_ACTIVE         = 4'b0011,
    CMD_PRECHARGE_ALL       = 4'b0010,
    CMD_AUTO_REFRESH        = 4'b0001,
    CMD_MODE_REGISTER_SET   = 4'b0000;

//==============State Machine=================
localparam
    STATE_INIT_WAIT = 8'd0,
    STATE_INIT_SETUP = 8'd1,
    STATE_IDLE = 8'd2;

reg [7:0] state = STATE_INIT_WAIT;





// wire [12:0] addr_row;
// wire [8:0]  addr_col;
// wire [1:0]  addr_bank;
// assign addr_col  = {cpu_addr[5:0], 3'd0}; // Make sure we start at the beginning of a burst
// assign addr_row  = cpu_addr[18:6];
// assign addr_bank = cpu_addr[20:19];

always @(posedge clk)
begin
    if (reset)
    begin
        cpu_done <= 1'b0;
        cpu_q <= 256'd0;

        SDRAM_A     <= 13'd0;
        SDRAM_BA    <= 2'b00;
        SDRAM_DQM   <= 4'b0000;

        refresh_counter <= 10'd0;
        startup_counter <= 16'd0;

        SDRAM_DATA  <= 32'd0;
        SDRAM_DQ_OE <= 1'b0;
        SDRAM_CMD   <= CMD_NOP;

        state <= STATE_INIT_WAIT;
    end
    else
    begin
        // Default assignments
        cpu_done    <= 1'b0;

        SDRAM_CMD   <= CMD_NOP;
        SDRAM_A     <= 13'd0;
        SDRAM_BA    <= 2'b00;
        SDRAM_DQM   <= 4'b0000;
        SDRAM_DATA  <= 32'd0;
        SDRAM_DQ_OE <= 1'b0;

        // Always update refresh counter
        refresh_counter <= refresh_counter + 1'b1;

        case (state)
            
            STATE_INIT_WAIT:
            begin
                SDRAM_DQM   <= 4'b1111; // DQM high until idle
                // Wait until startup_counter reaches sdram_startup_cycles
                if (startup_counter < sdram_startup_cycles)
                begin
                    startup_counter <= startup_counter + 1'b1;
                end
                else
                begin
                    // Move to precharge state
                    state <= STATE_INIT_SETUP;
                    startup_counter <= 16'd0; // Reset counter for next state
                end
            end

            STATE_INIT_SETUP:
            begin
                // This state does the following in sequence with 8 cycle delay between commands:
                // - Precharge all banks
                // - 8x Auto refresh
                // - Mode register set
                SDRAM_DQM <= 4'b1111; // DQM high until idle
                
                // Increment counter each cycle
                startup_counter <= startup_counter + 1'b1;
                
                case (startup_counter)
                    16'd0:
                    begin
                        // Precharge all banks
                        SDRAM_CMD <= CMD_PRECHARGE_ALL;
                        SDRAM_A[PRECHARGE_ADDR_IDX] <= 1'b1;
                    end
                    16'd8, 16'd16, 16'd24, 16'd32, 16'd40, 16'd48, 16'd56, 16'd64:
                    begin
                        // Auto refresh (8 times)
                        SDRAM_CMD <= CMD_AUTO_REFRESH;
                    end
                    16'd72:
                    begin
                        // Mode register set
                        SDRAM_CMD <= CMD_MODE_REGISTER_SET;
                        SDRAM_A <= MODE_REG;
                    end
                    16'd80:
                    begin
                        // Initialization done, move to idle
                        state <= STATE_IDLE;
                    end
                    default: begin
                        // NOP for all other cycles (already set by default assignments)
                    end
                endcase
            end

            STATE_IDLE:
            begin
                // Refresh has highest priority
                if (refresh_counter > cycles_per_refresh)
                begin
                    SDRAM_CMD <= CMD_AUTO_REFRESH;
                    refresh_counter <= 0;
                    // TODO: go to a state to wait tRC (60ns)
                end
                // TODO: else, handle read/write requests from CPU
            end
            default:
            begin
                state <= STATE_INIT_WAIT;
            end
        endcase
    end
end


endmodule
