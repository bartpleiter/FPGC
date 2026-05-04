/*
 * CameraCapture
 * -------------
 * OV7670 pixel capture engine for the FPGC-Camera project.
 *
 * Captures YUV422 data from the OV7670 parallel interface, extracts the
 * Y (luminance) channel, and writes complete 32-byte (256-bit) cache lines
 * to SDRAM via the SDRAMarbiter protocol.
 *
 * Clock domain crossing:
 *   The OV7670 pixel clock (cam_pclk, up to 25 MHz) is asynchronous to the
 *   system clock (clk, 100 MHz). We use a dual-clock async FIFO to safely
 *   transfer Y bytes from the pclk domain to the clk domain.
 *
 * Y channel extraction:
 *   OV7670 in YUV422 mode outputs bytes in order: Y0, U0, Y1, V0, Y2, U2, ...
 *   Every even byte (byte_count[0] == 0) when HREF is high is a Y value.
 *   We push only Y bytes into the FIFO.
 *
 * Cache line assembly:
 *   In the clk domain, we drain the FIFO and accumulate 32 Y bytes into a
 *   256-bit shift register. When 32 bytes are collected, we pulse sd_start
 *   and wait for sd_done, then increment the SDRAM address.
 *
 * Double buffering:
 *   Two frame buffers in SDRAM. On each VSYNC (start of vertical blanking),
 *   we toggle the active buffer and reset the write address.
 *
 * Variable frame rate:
 *   The module is entirely driven by sensor timing. Night mode (7.5-60 fps)
 *   just means VSYNC pulses arrive less often. The module doesn't care.
 */
module CameraCapture (
    // System
    input  wire         clk,            // 100 MHz system clock
    input  wire         reset,

    // OV7670 sensor interface (directly from FPGA pins)
    input  wire         cam_pclk,       // Pixel clock from sensor (~25 MHz)
    input  wire         cam_vsync,      // Vertical sync (HIGH = blanking)
    input  wire         cam_href,       // Horizontal reference (HIGH = valid pixel data)
    input  wire [7:0]   cam_data,       // 8-bit parallel pixel data

    // SDRAM write port (matches SDRAMarbiter dma_* protocol)
    output reg  [20:0]  sd_addr    = 21'd0,
    output reg  [255:0] sd_data    = 256'd0,
    output reg          sd_we      = 1'b1,    // Always write
    output reg          sd_start   = 1'b0,
    input  wire         sd_done,

    // Control (active in clk domain, from MMIO registers)
    input  wire         ctrl_enable,     // Master enable
    input  wire [20:0]  ctrl_base_buf0,  // SDRAM base addr for buffer 0
    input  wire [20:0]  ctrl_base_buf1,  // SDRAM base addr for buffer 1

    // Status (active in clk domain)
    output reg          frame_done   = 1'b0,  // 1-cycle pulse
    output reg          current_buf  = 1'b0   // Which buffer is being written
);

// =========================================================================
// Async FIFO (dual-clock, 16 entries × 8 bits)
// =========================================================================
// We use Gray-coded pointers for safe clock domain crossing.

reg  [3:0] wr_ptr_bin  = 4'd0;  // pclk domain
reg  [3:0] wr_ptr_gray = 4'd0;
reg  [3:0] rd_ptr_bin  = 4'd0;  // clk domain
reg  [3:0] rd_ptr_gray = 4'd0;

// Synchronised pointers (2-stage for metastability)
reg  [3:0] wr_gray_sync1 = 4'd0, wr_gray_sync2 = 4'd0;  // clk domain
reg  [3:0] rd_gray_sync1 = 4'd0, rd_gray_sync2 = 4'd0;  // pclk domain

// FIFO storage (16 entries × 8 bits, addressed by lower 4 bits of binary pointer)
reg  [7:0] fifo_mem [0:15];

wire fifo_empty = (rd_ptr_gray == wr_gray_sync2);
wire fifo_full  = (wr_ptr_gray == {~rd_gray_sync2[3:2], rd_gray_sync2[1:0]});

// Binary ↔ Gray conversions
function [3:0] bin2gray(input [3:0] b);
    bin2gray = b ^ (b >> 1);
endfunction

function [3:0] gray2bin(input [3:0] g);
    reg [3:0] b;
    begin
        b[3] = g[3];
        b[2] = g[3] ^ g[2];
        b[1] = g[3] ^ g[2] ^ g[1];
        b[0] = g[3] ^ g[2] ^ g[1] ^ g[0];
        gray2bin = b;
    end
endfunction

// --- Write side (pclk domain) ---
reg byte_toggle = 1'b0;  // 0 = Y byte, 1 = U/V byte

always @(posedge cam_pclk or posedge reset) begin
    if (reset) begin
        wr_ptr_bin  <= 4'd0;
        wr_ptr_gray <= 4'd0;
        byte_toggle <= 1'b0;
    end else if (ctrl_enable && cam_href) begin
        if (!byte_toggle && !fifo_full) begin
            // Even byte = Y channel → push to FIFO
            fifo_mem[wr_ptr_bin[3:0]] <= cam_data;
            wr_ptr_bin  <= wr_ptr_bin + 1'b1;
            wr_ptr_gray <= bin2gray(wr_ptr_bin + 1'b1);
        end
        byte_toggle <= ~byte_toggle;
    end else begin
        // Reset byte counter at start of each line
        byte_toggle <= 1'b0;
    end
end

// Sync write pointer into clk domain
always @(posedge clk) begin
    wr_gray_sync1 <= wr_ptr_gray;
    wr_gray_sync2 <= wr_gray_sync1;
end

// Sync read pointer into pclk domain
always @(posedge cam_pclk) begin
    rd_gray_sync1 <= rd_ptr_gray;
    rd_gray_sync2 <= rd_gray_sync1;
end

// --- Read side (clk domain) ---
wire [7:0] fifo_rd_data = fifo_mem[rd_ptr_bin[3:0]];
reg  fifo_rd_en = 1'b0;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        rd_ptr_bin  <= 4'd0;
        rd_ptr_gray <= 4'd0;
    end else if (fifo_rd_en) begin
        rd_ptr_bin  <= rd_ptr_bin + 1'b1;
        rd_ptr_gray <= bin2gray(rd_ptr_bin + 1'b1);
    end
end

// Register FIFO read data: capture on the cycle fifo_rd_en is high,
// use on the next cycle when fifo_rd_valid is high.
reg [7:0] fifo_rd_data_reg = 8'd0;
always @(posedge clk) begin
    if (fifo_rd_en)
        fifo_rd_data_reg <= fifo_rd_data;
end

// =========================================================================
// VSYNC edge detection (synchronise into clk domain)
// =========================================================================
reg vsync_sync1 = 1'b0, vsync_sync2 = 1'b0, vsync_prev = 1'b0;

always @(posedge clk) begin
    vsync_sync1 <= cam_vsync;
    vsync_sync2 <= vsync_sync1;
    vsync_prev  <= vsync_sync2;
end

wire vsync_rising = vsync_sync2 && !vsync_prev;  // Start of blanking

// =========================================================================
// Main state machine (clk domain)
// =========================================================================
localparam S_IDLE       = 3'd0;
localparam S_DRAIN      = 3'd1;  // Draining FIFO, assembling cache line
localparam S_WRITE      = 3'd2;  // Waiting for sd_done
localparam S_FRAME_DONE = 3'd3;  // Signal frame completion

reg [2:0]  state     = S_IDLE;
reg [4:0]  byte_cnt  = 5'd0;   // 0-31: bytes accumulated in cache line
reg        sdram_wait = 1'b0;   // Waiting for sd_done after sd_start
reg        fifo_rd_valid = 1'b0; // Delayed fifo_rd_en: data is valid this cycle

always @(posedge clk or posedge reset) begin
    if (reset) begin
        state         <= S_IDLE;
        sd_start      <= 1'b0;
        frame_done    <= 1'b0;
        current_buf   <= 1'b0;
        byte_cnt      <= 5'd0;
        fifo_rd_en    <= 1'b0;
        fifo_rd_valid <= 1'b0;
        sd_addr       <= 21'd0;
        sdram_wait    <= 1'b0;
    end else begin
        // Default: deassert single-cycle pulses
        sd_start   <= 1'b0;
        frame_done <= 1'b0;
        fifo_rd_en <= 1'b0;
        fifo_rd_valid <= fifo_rd_en;  // Data valid one cycle after rd_en

        // When fifo_rd_valid, the pointer has advanced and fifo_rd_data
        // from the PREVIOUS cycle is now stable — capture it.
        if (fifo_rd_valid && state == S_DRAIN) begin
            sd_data  <= {sd_data[247:0], fifo_rd_data_reg};
            byte_cnt <= byte_cnt + 1'b1;
            if (byte_cnt == 5'd31) begin
                state <= S_WRITE;
            end
        end

        case (state)
        S_IDLE: begin
            if (ctrl_enable && vsync_rising) begin
                // Wait for first VSYNC before capturing to avoid writing to addr 0
                current_buf <= 1'b0;
                sd_addr     <= ctrl_base_buf0;
                byte_cnt    <= 5'd0;
                state       <= S_DRAIN;
            end
        end

        S_DRAIN: begin
            if (!ctrl_enable) begin
                state <= S_IDLE;
            end else if (vsync_rising) begin
                // New frame: toggle buffer, reset address
                current_buf <= ~current_buf;
                sd_addr     <= current_buf ? ctrl_base_buf0 : ctrl_base_buf1;
                byte_cnt    <= 5'd0;
                state <= S_FRAME_DONE;
            end else if (!fifo_empty && !fifo_rd_en) begin
                // Issue read request; data captured next cycle via fifo_rd_valid
                fifo_rd_en <= 1'b1;
            end
        end

        S_WRITE: begin
            // Fire SDRAM write request (single pulse)
            if (!sd_start && !sdram_wait) begin
                sd_start   <= 1'b1;
                sdram_wait <= 1'b1;
                sd_we      <= 1'b1;
            end
            if (sd_done) begin
                sd_addr    <= sd_addr + 1'b1;
                byte_cnt   <= 5'd0;
                sdram_wait <= 1'b0;
                state      <= S_DRAIN;
            end
        end

        S_FRAME_DONE: begin
            frame_done <= 1'b1;
            state      <= S_DRAIN;
        end

        default: state <= S_IDLE;
        endcase
    end
end

endmodule
