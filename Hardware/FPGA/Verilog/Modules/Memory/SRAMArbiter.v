/*
 * SRAMArbiter
 * Arbiter for external SRAM access between CPU writes and GPU reads
 * 
 * Key design principles:
 * - Runs at 100MHz (4× GPU clock, 2× CPU clock)
 * - GPU reads directly from SRAM
 * - CPU writes only during blanking periods
 * - Continuous SRAM reads during active video for minimal latency
 * 
 * Operation:
 * - During GPU blanking: drain write FIFO to SRAM
 * - During active video: continuously read current GPU address from SRAM
 * 
 * Clock relationships (all from same PLL, phase-aligned):
 * - clk100: 100MHz arbiter clock (this module)
 * - GPU: 25MHz (1:4 ratio)
 * - CPU: 50MHz (1:2 ratio)
 * 
 * SRAM timing:
 * - IS61LV5128AL has 10ns access time
 * - At 100MHz (10ns period), we have exactly 1 cycle for read/write
 * 
 * GPU read approach:
 * - During active video, continuously output current GPU address to SRAM
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
        
        if (blank) begin
            //=================================================================
            // BLANKING PERIOD: Process CPU writes
            // But also keep reading GPU address in last few cycles before active
            // to prime the data for first pixel
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
            // ACTIVE VIDEO: Read for GPU
            //=================================================================
            // Reset write state when exiting blanking
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
