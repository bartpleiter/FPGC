/*
 * SRAMArbiter
 * Manages access to external SRAM between CPU writes and GPU reads
 * 
 * Uses time-division multiplexing:
 * - Even cycles: GPU read (prefetch pixels into GPU FIFO)
 * - Odd cycles: CPU write (if pending in write FIFO)
 * 
 * The arbiter maintains its own pixel address counter that follows
 * the same pattern as PixelEngine, but runs independently to prefetch
 * pixels ahead of GPU consumption.
 */
module SRAMArbiter (
    input  wire         clk,           // 50MHz system clock
    input  wire         reset,
    
    // GPU timing signals (synced to 50MHz domain)
    input  wire         vsync,         // Vertical sync
    input  wire [11:0]  h_count,       // Horizontal position (50MHz synced)
    input  wire [11:0]  v_count,       // Vertical position (50MHz synced)
    input  wire         halfRes,       // Half resolution mode
    
    // CPU Write FIFO interface
    input  wire [16:0]  cpu_wr_addr,
    input  wire [7:0]   cpu_wr_data,
    input  wire         cpu_fifo_empty,
    output reg          cpu_fifo_rd_en,
    
    // GPU Read FIFO interface
    output reg  [7:0]   gpu_rd_data,
    output reg          gpu_fifo_wr_en,
    input  wire         gpu_fifo_full,
    
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

// GPU prefetch state
reg [16:0] gpu_pixel_addr = 17'd0;  // Current pixel address to prefetch
reg gpu_frame_active = 1'b0;        // True when we're prefetching a frame
reg vsync_prev = 1'b0;              // For edge detection

// Frame size
localparam FRAME_PIXELS = 320 * 240;  // 76800 pixels

// State machine for SRAM access
// Simple 2-cycle operation: setup address, capture data
localparam STATE_IDLE       = 2'd0;
localparam STATE_GPU_WAIT   = 2'd1;  // GPU read - wait for data
localparam STATE_CPU_WRITE  = 2'd2;  // CPU write in progress

reg [1:0] state = STATE_IDLE;

always @(posedge clk) begin
    if (reset) begin
        state <= STATE_IDLE;
        gpu_pixel_addr <= 17'd0;
        gpu_frame_active <= 1'b0;
        vsync_prev <= 1'b0;
        cpu_fifo_rd_en <= 1'b0;
        gpu_fifo_wr_en <= 1'b0;
        sram_we_n <= 1'b1;
        sram_oe_n <= 1'b1;
        sram_addr <= 19'd0;
        sram_dq_out <= 8'd0;
    end else begin
        // Default: no FIFO operations
        cpu_fifo_rd_en <= 1'b0;
        gpu_fifo_wr_en <= 1'b0;
        
        // Edge detection for vsync (falling edge = start of new frame)
        vsync_prev <= vsync;
        
        // Start new frame prefetch on vsync falling edge
        if (vsync_prev && !vsync) begin
            gpu_pixel_addr <= 17'd0;
            gpu_frame_active <= 1'b1;
        end
        
        // Stop prefetching when we've fetched all pixels
        if (gpu_pixel_addr >= FRAME_PIXELS) begin
            gpu_frame_active <= 1'b0;
        end
        
        case (state)
            STATE_IDLE: begin
                // Default SRAM signals to idle
                sram_we_n <= 1'b1;
                sram_oe_n <= 1'b1;
                
                // Priority: GPU reads first (to keep display fed), then CPU writes
                if (gpu_frame_active && !gpu_fifo_full) begin
                    // Start GPU read - set up address and enable OE
                    sram_addr <= {2'b00, gpu_pixel_addr};
                    sram_oe_n <= 1'b0;
                    sram_we_n <= 1'b1;
                    gpu_pixel_addr <= gpu_pixel_addr + 1;
                    state <= STATE_GPU_WAIT;
                end else if (!cpu_fifo_empty) begin
                    // Start CPU write
                    sram_addr <= {2'b00, cpu_wr_addr};
                    sram_dq_out <= cpu_wr_data;
                    sram_we_n <= 1'b0;
                    sram_oe_n <= 1'b1;
                    cpu_fifo_rd_en <= 1'b1;
                    state <= STATE_CPU_WRITE;
                end
            end
            
            STATE_GPU_WAIT: begin
                // Data should be valid now - capture it
                // Keep OE low for this cycle to ensure valid data
                sram_oe_n <= 1'b0;
                
                if (!gpu_fifo_full) begin
                    gpu_rd_data <= sram_dq_in;
                    gpu_fifo_wr_en <= 1'b1;
                end
                // Return to idle
                state <= STATE_IDLE;
            end
            
            STATE_CPU_WRITE: begin
                // Write cycle complete (WE was low for one cycle)
                sram_we_n <= 1'b1;
                sram_oe_n <= 1'b1;
                state <= STATE_IDLE;
            end
            
            default: begin
                state <= STATE_IDLE;
                sram_we_n <= 1'b1;
                sram_oe_n <= 1'b1;
            end
        endcase
    end
end

endmodule
