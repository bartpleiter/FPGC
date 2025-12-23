/*
 * IS61LV5128AL
 * Simulation model for IS61LV5128AL 512K x 8 High-Speed CMOS Static RAM
 * 
 * This is a simplified behavioral model for simulation purposes.
 * Actual timing characteristics:
 *   - Trc (Read Cycle Time): 10ns min
 *   - Twc (Write Cycle Time): 10ns min
 *   - Taa (Address Access Time): 10ns max
 *   - Toh (Output Hold Time): 3ns min
 *   
 * Note: For simulation, we only model the lower 128KB to speed up compilation.
 * The pixel framebuffer only needs 76,800 bytes anyway.
 */
module IS61LV5128AL #(
    parameter LIST = ""  // Optional initialization file
) (
    input  wire [18:0] A,       // Address inputs (512K locations)
    inout  wire [7:0]  DQ,      // Data I/O
    input  wire        CE_n,    // Chip Enable (active low)
    input  wire        OE_n,    // Output Enable (active low)
    input  wire        WE_n     // Write Enable (active low)
);

// Memory array - 80K x 8 bits (slightly larger than pixel framebuffer)
// Using smaller size for faster Icarus Verilog compilation
reg [7:0] mem [0:81919];

// Use lower 17 bits of address for simulation
wire [16:0] sim_addr = A[16:0];

// Output data register
reg [7:0] data_out;

// Tristate control for bidirectional data bus
wire output_enable = ~CE_n && ~OE_n && WE_n;

assign DQ = output_enable ? data_out : 8'bz;

// Read operation (combinatorial - data available after address access time)
always @(*) begin
    if (~CE_n && ~OE_n && WE_n) begin
        data_out = mem[sim_addr];
    end else begin
        data_out = 8'bx;
    end
end

// Write operation - sample data on rising edge of WE_n (end of write cycle)
// This matches real SRAM behavior where data is latched at end of write pulse
reg we_n_prev = 1'b1;
always @(posedge WE_n or posedge CE_n) begin
    if (CE_n) begin
        // Chip disabled, do nothing
    end else begin
        // WE_n rising edge while CE_n is low - complete the write
        mem[sim_addr] <= DQ;
    end
end

// Initialize memory to 0 for simulation, then optionally load from file
integer i;
initial begin
    // Initialize all memory to 0 to avoid 'x' values in simulation
    for (i = 0; i < 81920; i = i + 1) begin
        mem[i] = 8'h00;
    end
    // Load from file if specified (overrides initialization)
    if (LIST != "") begin
        $readmemb(LIST, mem);
    end
end

// Debug output (optional, can be commented out for cleaner simulation)
`ifdef SRAM_DEBUG
always @(posedge WE_n) begin
    if (~CE_n) begin
        $display("%t SRAM Write: addr=%h data=%h", $time, sim_addr, DQ);
    end
end
always @(A or CE_n or OE_n or WE_n) begin
    if (~CE_n && ~OE_n && WE_n) begin
        $display("%t SRAM Read: addr=%h data=%h", $time, sim_addr, data_out);
    end
end
`endif

endmodule
