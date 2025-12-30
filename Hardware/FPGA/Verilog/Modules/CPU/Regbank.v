/*
 * Regbank
 * Contains 16 registers of 32 bit each
 * Reg0 is always 0
 * 
 * Fully registered design for optimal FPGA timing:
 * - Addresses are registered on input
 * - Data outputs are registered
 * - Total latency: 2 cycles from address to data
 * - Write-through forwarding handles WB conflicts
 */
module Regbank (
    // Clock and reset
    input wire clk,
    input wire reset,

    // Read ports (active during IF stage, data available in EX stage)
    input wire  [3:0]   addr_a,
    input wire  [3:0]   addr_b,
    input wire          clear,
    input wire          hold,
    output reg  [31:0]  data_a,
    output reg  [31:0]  data_b,

    // Write port (WB stage)
    input wire  [3:0]   addr_d,
    input wire  [31:0]  data_d,
    input wire          we
);

// 16 registers as individual flip-flops (distributed, not BRAM)
reg [31:0] regs [1:15];  // Skip reg0, it's always 0

// Stage 1: Registered addresses (captured in IF stage, used in ID stage)
reg [3:0] addr_a_reg = 4'd0;
reg [3:0] addr_b_reg = 4'd0;

// Stage 2: Registered read data (captured in ID stage, output in EX stage)
// This is the data_a/data_b outputs

// Read from register file using registered addresses (combinational)
wire [31:0] reg_read_a = (addr_a_reg == 4'd0) ? 32'd0 : regs[addr_a_reg];
wire [31:0] reg_read_b = (addr_b_reg == 4'd0) ? 32'd0 : regs[addr_b_reg];

// Write-through forwarding: if reading same register being written, forward new value
// Check against registered addresses since that's what we're reading
wire forward_a = we && (addr_d != 4'd0) && (addr_a_reg == addr_d);
wire forward_b = we && (addr_d != 4'd0) && (addr_b_reg == addr_d);

// Compute next output values (combinational)
wire [31:0] next_data_a = forward_a ? data_d : reg_read_a;
wire [31:0] next_data_b = forward_b ? data_d : reg_read_b;

// Sequential logic - two pipeline stages
always @(posedge clk) begin
    if (reset) begin
        addr_a_reg <= 4'd0;
        addr_b_reg <= 4'd0;
        data_a <= 32'd0;
        data_b <= 32'd0;
    end else if (clear) begin
        // Clear creates pipeline bubble - reset both stages
        addr_a_reg <= 4'd0;
        addr_b_reg <= 4'd0;
        data_a <= 32'd0;
        data_b <= 32'd0;
    end else if (!hold) begin
        // Stage 1: Register the addresses
        addr_a_reg <= addr_a;
        addr_b_reg <= addr_b;
        
        // Stage 2: Register the data outputs
        data_a <= next_data_a;
        data_b <= next_data_b;
    end
    // On hold: keep current state (both addresses and data)

    // Write to register file (reg0 writes are ignored)
    // Writes happen independently of hold/clear
    if (we && (addr_d != 4'd0)) begin
        regs[addr_d] <= data_d;
        $display("%0t reg r%02d: %d", $time, addr_d, data_d);
    end
end

`ifdef __ICARUS__
// Initialize all registers to 0
integer i;
initial begin
    for (i = 1; i < 16; i = i + 1) begin
        regs[i] = 32'd0;
    end
    addr_a_reg = 4'd0;
    addr_b_reg = 4'd0;
    data_a = 32'd0;
    data_b = 32'd0;
end
`endif

endmodule
