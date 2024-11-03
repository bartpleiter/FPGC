/*
* Instruction Memory
* Types of memory to be accessed:
* 1. ROM (single cycle)
* 2. L1 Instruction Cache (2 100MHz cycles at cache hit, longer at cache miss)
*
* Assumes SDRAM address starts at 0 and ends before or at SDARM_END, after which ROM starts somewhere after
* Assuming the number of ROM addresses are a power of 2,
*  this will work as long as the ROM starts at a multiple of the number of ROM addresses
*/

module InstrMem(
    input clk, clk100,
    input reset,
    input [26:0] addr,
    output [31:0] q,
    output hit,

    output [22:0]   bus_l1i_addr,
    output reg      bus_l1i_start = 1'b0,
    input [31:0]    bus_l1i_q,
    input           bus_l1i_done,
    input           bus_l1i_ready,

    output [8:0]    bus_i_rom_addr,
    input [31:0]    bus_i_rom_q,

    input clear, hold
);

localparam STATE_IDLE = 3'b000;
localparam STATE_WAIT = 3'b001;

localparam SDRAM_END = 32'h1000000;

wire addr_rom = addr >= SDRAM_END;
wire addr_l1i = addr < SDRAM_END;

assign bus_l1i_addr = addr;
assign bus_i_rom_addr = addr;

assign hit = (addr_rom) ? 1'b1 : (bus_l1i_done | bus_l1i_done_latch);
assign q = (addr_rom) ? bus_i_rom_q : bus_l1i_q;

reg [2:0] state = STATE_IDLE;

reg bus_l1i_done_latch = 1'b0;

// TODO: Implement clear/ignore next logic in state machine


always @(posedge clk100)
begin
    if (reset)
    begin
        bus_l1i_start <= 1'b0;
        state <= STATE_IDLE;
        bus_l1i_done_latch <= 1'b0;
    end
    else
    begin
        bus_l1i_done_latch <= bus_l1i_done;
        case (state)
            STATE_IDLE:
            begin
                if (addr_l1i)
                begin
                    if (bus_l1i_ready && !clear && !hold)
                    begin
                        bus_l1i_start <= 1'b1;
                        state <= STATE_WAIT;
                    end
                end
            end
            STATE_WAIT:
            begin
                bus_l1i_start <= 1'b0;
                if (bus_l1i_done)
                begin
                    if (bus_l1i_ready && !clear && !hold)
                    begin
                        bus_l1i_start <= 1'b1;
                        state <= STATE_WAIT;
                    end
                    else
                    begin
                        state <= STATE_IDLE;
                    end
                end
            end
        endcase
    end
end

endmodule