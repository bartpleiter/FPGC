/*
 * SRAMArbiter
 * Arbiter for external SRAM access between CPU writes and display reads
 * 
 * Simplified for SPI display: no VGA timing dependency.
 * The display controller reads pixels at ~1.5 Mpixel/s (much slower
 * than the 100 MHz arbiter clock), so CPU writes get ~98% of bandwidth.
 * 
 * Priority:
 *   1. Display read (when display_read asserted) — 1 cycle
 *   2. CPU write from FIFO — 3-cycle state machine
 *   3. Idle — keep reading display address for low latency
 * 
 * SRAM timing:
 * - IS61LV5128AL has 10ns access time
 * - At 100MHz (10ns period), we have exactly 1 cycle for read/write
 */
module SRAMArbiter (
    input  wire         clk100,        // 100MHz arbiter clock
    input  wire         reset,
    
    // Display read interface
    input  wire [16:0]  gpu_addr,      // Current pixel address request
    output wire [7:0]   gpu_data,      // Pixel data output
    input  wire         display_read,  // Asserted when display needs a read
    
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

// Register SRAM data for stable output
reg [7:0] sram_data_reg = 8'd0;
assign gpu_data = sram_data_reg;

// ---- CPU write state machine ----
localparam
    STATE_IDLE       = 2'd0,
    STATE_WRITE_WAIT = 2'd1,  // Wait one cycle for FIFO data
    STATE_WRITE_EXEC = 2'd2;  // Execute SRAM write

reg [1:0] write_state = STATE_IDLE;

// Write in progress: must complete current write even if display_read arrives
wire write_in_progress = (write_state != STATE_IDLE);

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
        
        if (write_in_progress) begin
            // ---- Complete in-progress write ----
            case (write_state)
                STATE_WRITE_WAIT: begin
                    // FIFO data now valid
                    sram_addr <= {2'b00, cpu_wr_addr};
                    sram_dq_out <= cpu_wr_data;
                    sram_we_n <= 1'b0;
                    sram_oe_n <= 1'b1;
                    write_state <= STATE_WRITE_EXEC;
                end
                
                STATE_WRITE_EXEC: begin
                    sram_we_n <= 1'b1;
                    write_state <= STATE_IDLE;
                end
                
                default: begin
                    write_state <= STATE_IDLE;
                    sram_we_n <= 1'b1;
                end
            endcase
        end else if (!cpu_fifo_empty && !display_read) begin
            // ---- Start CPU write (only when display not reading) ----
            cpu_fifo_rd_en <= 1'b1;
            sram_oe_n <= 1'b1;
            write_state <= STATE_WRITE_WAIT;
        end else begin
            // ---- Read for display (default) ----
            sram_addr <= {2'b00, gpu_addr};
            sram_oe_n <= 1'b0;
            sram_we_n <= 1'b1;
            sram_data_reg <= sram_dq_in;
        end
    end
end

endmodule
