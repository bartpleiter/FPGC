/*
* SDRAM controller
* Custom made for FPGC with l1 cache (one word per cache line), having two W9825G6KH-6 chips:
*   - bus interface to be connected directly to arbiter, so no memory unit in between
*   - no burst, so high latency access to single words
*/
module SDRAMcontroller(
    // clock/reset inputs
    input               clk,
    input               reset,

    // signal inputs
    input [23:0]        sdc_addr,   // bus_addr
    input [31:0]        sdc_data,   // bus_data
    input               sdc_we,     // bus_we
    input               sdc_start,  // bus_start

    // signal outputs
    output reg [31:0]   sdc_q = 32'd0,      // bus_q
    output reg          sdc_done = 1'b0,    // bus_done

    // SDRAM signals, initialized for power up
    output              SDRAM_CSn, SDRAM_WEn, SDRAM_CASn, SDRAM_RASn,
    output reg          SDRAM_CKE = 1'b1, 
    output reg [12:0]   SDRAM_A   = 13'd0,
    output reg [1:0]    SDRAM_BA  = 2'd0,
    output reg [3:0]    SDRAM_DQM = 4'b1111,
    inout [31:0]        SDRAM_DQ
);

// SDRAM commands
parameter [3:0] SDRAM_CMD_UNSELECTED= 4'b1000;
parameter [3:0] SDRAM_CMD_NOP       = 4'b0111;
parameter [3:0] SDRAM_CMD_ACTIVE    = 4'b0011;
parameter [3:0] SDRAM_CMD_READ      = 4'b0101;
parameter [3:0] SDRAM_CMD_WRITE     = 4'b0100;
parameter [3:0] SDRAM_CMD_BURSTSTOP = 4'b0110;
parameter [3:0] SDRAM_CMD_PRECHARGE = 4'b0010;
parameter [3:0] SDRAM_CMD_REFRESH   = 4'b0001;
parameter [3:0] SDRAM_CMD_LOADMODE  = 4'b0000;

// Mode register value
// {3'b reserved, 1'b write mode, 1'b reserved, 1'b test mode, 3'b CAS latency, 1'b addressing mode, 3'b burst length}
//  CAS latency: 010=2, 011=3
//  addressing mode: 0=seq 1=interleave
//  burst length: 000=1, 001=2, 010=4, 011=8, 111=full page
parameter [12:0] MODE_REG = {3'b0, 1'b0, 1'b0, 1'b0, 3'b010, 1'b0, 3'b000};

// assign command pins to selected command
reg [3:0] SDRAM_CMD = SDRAM_CMD_NOP; // default to NOP for power up
assign {SDRAM_CSn, SDRAM_RASn, SDRAM_CASn, SDRAM_WEn} = SDRAM_CMD;


// cycles_per_refresh calculation:
//   25MHz -> 25.000.000 cycles per sec
//   25.000.000*0.064 -> 1.600.000 cycles per 64ms
//   1.600.000 / 8192 auto refreshes -> refresh after 196 cycles
//   for 50mhz this should be 196*2 cycles and for 100mhz this should be 196*4 cycles
parameter cycles_per_refresh = 784;
reg  [9:0] refresh_counter = 10'd0;     // cycle counter for refresh command, max 1023

parameter sdram_startup_cycles = 100;   // 200us @ 100MHz -> 20.000 cycles. Lowered for simulation
reg [15:0] startup_counter = 16'd0;     // cycle counter for startup, max 65.535

parameter addr_precharge_bit  = 10;     // address bit that selects which banks are to be precharged (1 == all banks)


// convert input address into row, column and bank
wire [12:0] addr_row;
wire [8:0]  addr_col;
wire [1:0]  addr_bank;
assign addr_col  = sdc_addr[8:0];   // 9  bit columns
assign addr_row  = sdc_addr[21:9];  // 13 bit rows
assign addr_bank = sdc_addr[23:22]; // 2  bit banks


// DQ port setup
// write
reg [31:0] SDRAM_DATA = 32'd0;
reg SDRAM_DQ_OE = 1'b0;
assign SDRAM_DQ = SDRAM_DQ_OE ? SDRAM_DATA : 32'hZZZZ;
// read
wire [31:0] SDRAM_Q;
assign SDRAM_Q = SDRAM_DQ;


// state machine
parameter s_init        = 5'd0;
parameter s_idle        = 5'd1;
parameter s_open_in_2   = 5'd2;
parameter s_open_in_1   = 5'd3;
parameter s_write_1     = 5'd4;
parameter s_write_2     = 5'd5;
parameter s_write_3     = 5'd6;
parameter s_read_1      = 5'd7;
parameter s_read_2      = 5'd8;
parameter s_read_3      = 5'd9;
parameter s_read_4      = 5'd10;
parameter s_write_precharge   = 5'd11;
parameter s_idle_in_6   = 5'd12;
parameter s_idle_in_5   = 5'd13;
parameter s_idle_in_4   = 5'd14;
parameter s_idle_in_3   = 5'd15;
parameter s_idle_in_2   = 5'd16;
parameter s_idle_in_1   = 5'd17;
parameter s_read_precharge = 5'd18;
parameter s_read_idle_in_3 = 5'd19;
reg [4:0] state = s_init;


// not used, but useful for debugging
wire refresh = (SDRAM_CMD == SDRAM_CMD_REFRESH);

reg is_refreshing = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        SDRAM_CMD <= SDRAM_CMD_UNSELECTED;
        state <= s_init;
        startup_counter <= 16'd0;
        sdc_done <= 1'b0;
    end
    else
    begin
        // default state
        SDRAM_CMD   <= SDRAM_CMD_NOP;
        SDRAM_A     <= 13'd0;
        SDRAM_BA    <= 2'b00;
        SDRAM_DQM   <= 4'b0000;
        sdc_done <= 1'b0;
     
        // update counter for refresh
        refresh_counter <= refresh_counter + 1'b1;

        case(state)
            s_init: 
            begin
                // ~200us pause,
                //  followed by precharge of all banks,
                //  followed by two auto refreshes,
                //  followed by mode register init
                SDRAM_CKE <= 1'b1;

                // hold DQM high during initial pause
                if (startup_counter < sdram_startup_cycles - 50)
                begin
                    SDRAM_DQM <= 4'b1111;
                end

                case(startup_counter)
                    (sdram_startup_cycles-50):
                    begin
                        // precharge all banks
                        SDRAM_CMD <= SDRAM_CMD_PRECHARGE;
                        SDRAM_A[addr_precharge_bit] <= 1'b1; // select all banks
                        SDRAM_BA <= 2'b00;
                    end
                    (sdram_startup_cycles-28):
                    begin
                        SDRAM_CMD <= SDRAM_CMD_REFRESH;
                    end
                    (sdram_startup_cycles-18):
                    begin
                        SDRAM_CMD <= SDRAM_CMD_REFRESH;
                    end
                    (sdram_startup_cycles-7):
                    begin
                        SDRAM_CMD <= SDRAM_CMD_LOADMODE;
                        SDRAM_A <= MODE_REG;
                    end
                    sdram_startup_cycles:
                    begin
                        state <= s_idle;
                    end
                    default: 
                    begin
                    end
                endcase
                startup_counter <= startup_counter + 1'b1;
            end

            s_idle_in_6: state <= s_idle_in_5;
            s_idle_in_5: state <= s_idle_in_4;
            s_idle_in_4: state <= s_idle_in_3;
            s_idle_in_3: state <= s_idle_in_2;
            s_idle_in_2: state <= s_idle_in_1;
            s_idle_in_1: state <= s_idle;

            s_idle:
            begin
                if (refresh_counter > cycles_per_refresh) //refresh has priority!
                    begin
                        state       <= s_idle_in_6;
                        is_refreshing <= 1'b1;
                        SDRAM_CMD   <= SDRAM_CMD_REFRESH;
                        refresh_counter <= 0;
                    end
                else 
                begin     
                    if (sdc_start)
                    begin
                        //--------------------------------
                        //-- Start the read or write cycle. 
                        //-- First task is to open the row
                        //--------------------------------
                        state       <= s_open_in_2;
                        SDRAM_CMD   <= SDRAM_CMD_ACTIVE;
                        SDRAM_A     <= addr_row;
                        SDRAM_BA    <= addr_bank;
                    end
                    else //if nothing happens, just nop
                    begin
                        SDRAM_DQM <= 2'b00;
                        SDRAM_CMD <= SDRAM_CMD_NOP;
                        SDRAM_A <= 0;  
                    end
                end                
                                
            end

            //--------------------------------------------
            //-- Opening the row ready for reads or writes
            //--------------------------------------------
            s_open_in_2: state <= s_open_in_1;

            s_open_in_1:
            begin
                // if write command
                if (sdc_we)
                begin
                    state <= s_write_1;
                    SDRAM_DATA <= sdc_data; // already present sdram with data
                    SDRAM_DQ_OE <= 1'b1;
                end
                else // if read command
                begin
                    state <= s_read_1;
                    SDRAM_DQ_OE <= 1'b0;
                end
            end

            s_write_1: 
            begin
                state                       <= s_write_2;
                SDRAM_CMD                   <= SDRAM_CMD_WRITE;
                SDRAM_A                     <= addr_col; 
                SDRAM_A[addr_precharge_bit] <= 1'b0;
                SDRAM_BA                    <= addr_bank;
                SDRAM_DQM                   <= 2'b00;  
                SDRAM_DATA                  <= sdc_data;
            end

            s_write_2:
            begin
                state                   <= s_write_precharge;
                SDRAM_DQ_OE             <= 1'b0;
                sdc_done             <= 1'b1; // high for two cycles (this + s_write_precharge)
            end

            s_write_precharge:
            begin
                sdc_done                 <= 1'b1;
                state                       <= s_idle_in_3;
                SDRAM_CMD                   <= SDRAM_CMD_PRECHARGE;
                SDRAM_A[addr_precharge_bit] <= 1'b1;
            end
            

            //----------------------------------
            //-- Processing the read transaction
            //----------------------------------
            s_read_1: 
            begin
                state                       <= s_read_2;
                SDRAM_CMD                   <= SDRAM_CMD_READ;
                SDRAM_A                     <= addr_col; 
                SDRAM_BA                    <= addr_bank;
                SDRAM_A[addr_precharge_bit] <= 1'b0;
                SDRAM_DQM                   <= 2'b00;
            end   
            s_read_2:
            begin
                state                       <= s_read_3;
            end   
            s_read_3:
            begin
                state                       <= s_read_precharge;
            end

            s_read_precharge:
            begin
                sdc_q                       <= SDRAM_Q;
                sdc_done                 <= 1'b1; // high for two cycles (this + s_read_idle_in_3)
                state                       <= s_read_idle_in_3;
                SDRAM_CMD                   <= SDRAM_CMD_PRECHARGE;
                SDRAM_A[addr_precharge_bit] <= 1'b1;
            end

            s_read_idle_in_3:
            begin
                sdc_done                 <= 1'b1;
                state                       <= s_idle_in_2;
            end
            
            default:
            begin
                SDRAM_CMD <= SDRAM_CMD_UNSELECTED;
                state <= s_init;
                startup_counter <= 16'd0;
            end
        endcase
    end
end
endmodule