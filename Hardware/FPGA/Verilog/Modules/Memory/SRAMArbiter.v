/*
 * SRAMArbiter
 * Arbiter for external SRAM access between CPU writes and GPU reads
 * 
 * Key design principles:
 * - Runs at 100MHz (4× GPU clock, same as CPU clock)
 * - GPU reads directly from SRAM (even lines only - odd lines use line buffer)
 * - CPU writes during blanking OR during odd lines (when GPU uses line buffer)
 * - This doubles the effective CPU write bandwidth
 * 
 * Operation:
 * - During GPU blanking: drain write FIFO to SRAM
 * - During active video, even lines: continuously read for GPU
 * - During active video, odd lines: drain write FIFO (GPU uses line buffer)
 * 
 * Clock relationships (all from same PLL, phase-aligned):
 * - clk100: 100MHz arbiter clock (this module)
 * - GPU: 25MHz (1:4 ratio)
 * - CPU: 100MHz (1:1 ratio)
 * 
 * SRAM timing:
 * - IS61LV5128AL has 10ns access time
 * - At 100MHz (10ns period), we have exactly 1 cycle for read/write
 * 
 * GPU read approach:
 * - During active video (even lines), continuously output current GPU address
 * - SRAM data is registered, providing stable output with 1-2 cycle latency
 */
module SRAMArbiter (
    input  wire         clk100,        // 100MHz arbiter clock
    input  wire         reset,
    
    // GPU interface (directly from PixelEngine, synced to 100MHz)
    input  wire [16:0]  gpu_addr,      // Current pixel address request
    output wire [7:0]   gpu_data,      // Pixel data output to GPU
    
    // GPU timing (directly from TimingGenerator)
    input  wire         blank,         // High during blanking period
    input  wire         vsync,         // For debug/monitoring
    input  wire         using_line_buffer, // High when GPU uses line buffer (SRAM free)
    
    // CPU Write FIFO interface (read side, 100MHz)
    input  wire [16:0]  cpu_wr_addr,
    input  wire [7:0]   cpu_wr_data,
    input  wire         cpu_fifo_empty,
    output reg          cpu_fifo_rd_en,
    
    // External SRAM interface
    output reg  [18:0]  sram_addr,
    output reg  [7:0]   sram_dq_out,
    input  wire [7:0]   sram_dq_in,
    output reg          sram_we_n,
    output reg          sram_oe_n,
    output wire         sram_cs_n
);

// SRAM is always enabled
assign sram_cs_n = 1'b0;

// Register SRAM data for stable output to GPU
reg [7:0] sram_data_reg = 8'd0;
assign gpu_data = sram_data_reg;

// Determine when we can process CPU writes:
// - During blanking (no GPU reads needed)
// - During odd lines when GPU uses line buffer (SRAM is free)
wire can_write = blank || using_line_buffer;

// CPU write state machine
localparam STATE_IDLE       = 2'd0;
localparam STATE_WRITE_WAIT = 2'd1;  // Wait one cycle for FIFO data
localparam STATE_WRITE_EXEC = 2'd2;  // Execute SRAM write

reg [1:0] write_state = STATE_IDLE;

always @(posedge clk100) begin
    if (reset) begin
        write_state <= STATE_IDLE;
        cpu_fifo_rd_en <= 1'b0;
        sram_addr <= 19'd0;
        sram_dq_out <= 8'd0;
        sram_we_n <= 1'b1;
        sram_oe_n <= 1'b1;
        sram_data_reg <= 8'd0;
    end else begin
        // Default: no FIFO read
        cpu_fifo_rd_en <= 1'b0;
        
        if (can_write) begin
            //=================================================================
            // WRITE PERIOD: Blanking OR GPU using line buffer (odd lines)
            // Process CPU writes from FIFO
            //=================================================================
            case (write_state)
                STATE_IDLE: begin
                    sram_we_n <= 1'b1;
                    
                    if (!cpu_fifo_empty) begin
                        // Request next FIFO entry
                        cpu_fifo_rd_en <= 1'b1;
                        sram_oe_n <= 1'b1;
                        write_state <= STATE_WRITE_WAIT;
                    end else begin
                        // No writes pending - keep reading GPU address to prime data
                        sram_addr <= {2'b00, gpu_addr};
                        sram_oe_n <= 1'b0;
                        sram_data_reg <= sram_dq_in;
                    end
                end
                
                STATE_WRITE_WAIT: begin
                    // FIFO data now valid (registered output)
                    // Set up SRAM write
                    sram_addr <= {2'b00, cpu_wr_addr};
                    sram_dq_out <= cpu_wr_data;
                    sram_we_n <= 1'b0;
                    sram_oe_n <= 1'b1;
                    write_state <= STATE_WRITE_EXEC;
                end
                
                STATE_WRITE_EXEC: begin
                    // Write complete, return to idle
                    sram_we_n <= 1'b1;
                    write_state <= STATE_IDLE;
                end
                
                default: begin
                    write_state <= STATE_IDLE;
                    sram_we_n <= 1'b1;
                end
            endcase
        end else begin
            //=================================================================
            // ACTIVE VIDEO (even lines): Read for GPU
            //=================================================================
            // Reset write state when entering GPU read mode
            write_state <= STATE_IDLE;
            
            // Continuously read current GPU address
            sram_addr <= {2'b00, gpu_addr};
            sram_oe_n <= 1'b0;
            sram_we_n <= 1'b1;
            
            // Capture SRAM data (1 cycle delay is inherent)
            sram_data_reg <= sram_dq_in;
        end
    end
end

endmodule
