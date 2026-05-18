/*
 * SRAMArbiter
 * Arbiter for external SRAM access between CPU writes and display reads
 * 
 * Simplified for SPI display: no VGA timing dependency.
 * The display controller reads pixels at ~1.5 Mpixel/s (much slower
 * than the 100 MHz arbiter clock), so CPU writes get ~98% of bandwidth.
 * 
 * Priority:
 *   1. Display read (when display_read asserted) — blocks new writes
 *   2. CPU write from FIFO — 3-cycle state machine
 *   3. Idle — keep reading display address for low latency
 * 
 * Read validation:
 *   gpu_data_valid goes HIGH only when sram_data_reg contains correct
 *   data for the current gpu_addr. It goes LOW when the address changes
 *   or during/after a write, until the data has settled.
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
    output wire         gpu_data_valid, // HIGH when gpu_data is correct for gpu_addr
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

// ---- Read validation logic ----
// Track when sram_data_reg actually contains valid data for current gpu_addr.
// After address change or write: need 1 cycle in read path for SRAM to respond,
// then 1 more cycle for data to be captured into sram_data_reg.
reg [16:0] prev_gpu_addr = 17'd0;
reg        addr_stable = 1'b0;
reg        was_writing = 1'b0; // Delays validation by 1 cycle after write

// Combinational: detect address change THIS cycle (before registered update)
wire addr_just_changed = (gpu_addr != prev_gpu_addr);

assign gpu_data_valid = addr_stable && !write_in_progress && !addr_just_changed;

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
        prev_gpu_addr <= 17'd0;
        addr_stable <= 1'b0;
        was_writing <= 1'b0;
    end else begin
        // Default: no FIFO read
        cpu_fifo_rd_en <= 1'b0;
        // Track write→idle transitions
        was_writing <= write_in_progress;
        
        if (write_in_progress) begin
            // ---- Complete in-progress write ----
            addr_stable <= 1'b0; // Data invalid during writes
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
            addr_stable <= 1'b0; // Invalidate during write
            write_state <= STATE_WRITE_WAIT;
        end else begin
            // ---- Read for display (default) ----
            sram_addr <= {2'b00, gpu_addr};
            sram_oe_n <= 1'b0;
            sram_we_n <= 1'b1;
            sram_data_reg <= sram_dq_in;
            
            // Track address stability for data validation.
            // Must see same address for 1 full cycle (with no prior write)
            // before data is considered valid.
            if (was_writing || gpu_addr != prev_gpu_addr) begin
                addr_stable <= 1'b0;
                prev_gpu_addr <= gpu_addr;
            end else begin
                addr_stable <= 1'b1;
            end
        end
    end
end

endmodule
