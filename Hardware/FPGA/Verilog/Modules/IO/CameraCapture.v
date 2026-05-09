/*
 * CameraCapture V2
 * ----------------
 * OV7670 pixel capture engine for the FPGC-Camera project.
 *
 * Captures YUV422 data from the OV7670 parallel interface, extracts the
 * Y (luminance) channel, and accumulates 32-byte (256-bit) cache lines.
 * Completed cache lines are exposed via a ready/ack handshake for the
 * DMA engine to consume and write to SDRAM.
 *
 * V2 changes (from fpgc-camera-poc-v1-review-and-v2-plan.md §5.3):
 *   - Explicit HREF edge detection for byte_toggle / byte_cnt reset
 *   - Line pixel counter (caps at 320 Y pixels per line)
 *   - Partial cache line discard at HREF falling edge
 *   - Extended debug counters (frame_pixels, line_count, partial_drops)
 *
 * Single-clock design:
 *   The OV7670 pixel clock (cam_pclk, ~25 MHz) is sampled as a DATA input
 *   in the 100 MHz system clock domain. Rising edges are detected via a
 *   2-stage synchronizer + edge detect.
 *
 * Handshake interface:
 *   - line_ready: asserted when a 256-bit cache line is available
 *   - line_data:  the 256-bit cache line
 *   - line_ack:   DMA engine asserts for 1 cycle to consume the line
 *   Pixel capture continues into the next cache line during the handshake.
 *   If DMA is too slow (line_ready still HIGH when next line completes),
 *   the old line is overwritten (dropped).
 *
 * Y channel extraction:
 *   OV7670 in YUV422 UYVY mode outputs bytes: U0, Y0, V0, Y1, ...
 *   Every odd byte (index 1, 3, 5...) when HREF is high is a Y value.
 *   byte_toggle selects odd bytes; it resets on HREF rising edge.
 *
 * Frame sync:
 *   On each VSYNC rising edge, frame_done pulses and current_buf toggles.
 *   The DMA engine uses frame_done to know when a frame boundary occurs.
 */
module CameraCapture (
    // System
    input  wire         clk,            // 100 MHz system clock
    input  wire         reset,

    // OV7670 sensor interface (directly from FPGA pins)
    input  wire         cam_pclk,       // Pixel clock from sensor (~25 MHz) — sampled as data
    input  wire         cam_vsync,      // Vertical sync (HIGH = blanking)
    input  wire         cam_href,       // Horizontal reference (HIGH = valid pixel data)
    input  wire [7:0]   cam_data,       // 8-bit parallel pixel data

    // Cache line handshake (to DMA engine)
    output reg  [255:0] line_data  = 256'd0,  // Completed cache line
    output reg          line_ready = 1'b0,    // HIGH when line_data is valid
    input  wire         line_ack,             // DMA consumed the line (1-cycle pulse)

    // Control (from MMIO registers, active in clk domain)
    input  wire         ctrl_enable,     // Master enable

    // Status (active in clk domain)
    output reg          frame_done   = 1'b0,  // 1-cycle pulse on VSYNC rising
    output reg          current_buf  = 1'b0,  // Toggles each frame

    // Debug outputs — latched at frame boundary so software reads stable values
    output wire [2:0]   dbg_state,
    output reg  [16:0]  dbg_frame_pixels  = 17'd0,  // Y pixels in last completed frame (expect 76800)
    output reg  [8:0]   dbg_line_count    = 9'd0,   // Lines in last completed frame (expect 240)
    output reg  [11:0]  dbg_cache_lines   = 12'd0,  // Cache lines in last completed frame (expect 2400)
    output reg  [7:0]   dbg_partial_drops = 8'd0    // Partial cache lines discarded in last frame
);

// =========================================================================
// Input synchronization (all sensor signals → clk domain)
// =========================================================================
reg pclk_s1 = 1'b0, pclk_s2 = 1'b0, pclk_s3 = 1'b0;
wire pclk_rising = pclk_s2 && !pclk_s3;

reg vsync_s1 = 1'b0, vsync_s2 = 1'b0, vsync_prev = 1'b0;
wire vsync_rising = vsync_s2 && !vsync_prev;

reg href_s1 = 1'b0, href_s2 = 1'b0, href_prev = 1'b0;
wire href_rising  = href_s2 && !href_prev;
wire href_falling = !href_s2 && href_prev;

reg [7:0] data_s1 = 8'd0, data_s2 = 8'd0;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        pclk_s1    <= 1'b0; pclk_s2    <= 1'b0; pclk_s3    <= 1'b0;
        vsync_s1   <= 1'b0; vsync_s2   <= 1'b0; vsync_prev <= 1'b0;
        href_s1    <= 1'b0; href_s2    <= 1'b0; href_prev  <= 1'b0;
        data_s1    <= 8'd0; data_s2    <= 8'd0;
    end else begin
        pclk_s1  <= cam_pclk;
        pclk_s2  <= pclk_s1;
        pclk_s3  <= pclk_s2;

        vsync_s1   <= cam_vsync;
        vsync_s2   <= vsync_s1;
        vsync_prev <= vsync_s2;

        href_s1    <= cam_href;
        href_s2    <= href_s1;
        href_prev  <= href_s2;

        data_s1  <= cam_data;
        data_s2  <= data_s1;
    end
end

// =========================================================================
// Pixel capture (on detected PCLK rising edge)
// =========================================================================
// YUV422 UYVY: bytes 0,2,4.. are U/V (discarded), bytes 1,3,5.. are Y (kept).
// byte_toggle alternates on each PCLK edge while HREF is high.
// Resets cleanly on HREF rising edge to ensure correct phase.
reg byte_toggle = 1'b0;
reg pixel_valid = 1'b0;
reg [7:0] pixel_byte = 8'd0;

// Line pixel counter: caps at 320 Y pixels per line (§5.3.3)
reg [8:0] line_pixel_cnt = 9'd0;
wire pixel_accept = pixel_valid && (line_pixel_cnt < 9'd320);

always @(posedge clk or posedge reset) begin
    if (reset) begin
        byte_toggle    <= 1'b0;
        pixel_valid    <= 1'b0;
        pixel_byte     <= 8'd0;
        line_pixel_cnt <= 9'd0;
    end else begin
        pixel_valid <= 1'b0;

        // HREF edge resets (§5.3.1) — evaluated even without pclk_rising
        if (href_rising) begin
            byte_toggle    <= 1'b0;
            line_pixel_cnt <= 9'd0;
        end

        if (pclk_rising && ctrl_enable) begin
            if (href_s2) begin
                if (byte_toggle) begin
                    pixel_byte  <= data_s2;
                    pixel_valid <= 1'b1;
                end
                byte_toggle <= ~byte_toggle;
            end
            // Note: byte_toggle reset on !href is handled by href_rising above
        end else if (!ctrl_enable) begin
            byte_toggle    <= 1'b0;
            line_pixel_cnt <= 9'd0;
        end

        // Count accepted Y pixels per line
        if (pixel_accept)
            line_pixel_cnt <= line_pixel_cnt + 1'b1;
    end
end

// =========================================================================
// Main state machine — cache line assembly + handshake
// =========================================================================
localparam S_IDLE       = 3'd0;
localparam S_CAPTURE    = 3'd1;  // Accumulating Y pixels into shift register
localparam S_FRAME_DONE = 3'd2;  // Signal frame completion

reg [2:0]   state      = S_IDLE;
reg [4:0]   byte_cnt   = 5'd0;
reg [255:0] line_accum = 256'd0;  // Shift register for current cache line

assign dbg_state = state;

// Running counters for current frame (latched into dbg_* at frame boundary)
reg [16:0] cur_frame_pixels  = 17'd0;
reg [8:0]  cur_line_count    = 9'd0;
reg [11:0] cur_cache_lines   = 12'd0;
reg [7:0]  cur_partial_drops = 8'd0;

// Partial cache line detection: on HREF falling edge, if byte_cnt > 0,
// we have an incomplete cache line that must be discarded.
reg href_fell = 1'b0;  // Registered to align with FSM timing

always @(posedge clk or posedge reset) begin
    if (reset)
        href_fell <= 1'b0;
    else
        href_fell <= href_falling && (state == S_CAPTURE);
end

always @(posedge clk or posedge reset) begin
    if (reset) begin
        state              <= S_IDLE;
        frame_done         <= 1'b0;
        current_buf        <= 1'b0;
        byte_cnt           <= 5'd0;
        line_accum         <= 256'd0;
        line_data          <= 256'd0;
        line_ready         <= 1'b0;
        cur_frame_pixels   <= 17'd0;
        cur_line_count     <= 9'd0;
        cur_cache_lines    <= 12'd0;
        cur_partial_drops  <= 8'd0;
        dbg_frame_pixels   <= 17'd0;
        dbg_line_count     <= 9'd0;
        dbg_cache_lines    <= 12'd0;
        dbg_partial_drops  <= 8'd0;
    end else begin
        frame_done <= 1'b0;

        // DMA consumed the cache line
        if (line_ack)
            line_ready <= 1'b0;

        // Discard partial cache line at end of scanline (§5.3.1)
        if (href_fell && byte_cnt != 5'd0) begin
            byte_cnt          <= 5'd0;
            line_accum        <= 256'd0;
            cur_partial_drops <= cur_partial_drops + 1'b1;
        end

        case (state)
        S_IDLE: begin
            if (ctrl_enable && vsync_rising) begin
                current_buf       <= 1'b0;
                byte_cnt          <= 5'd0;
                line_accum        <= 256'd0;
                cur_frame_pixels  <= 17'd0;
                cur_line_count    <= 9'd0;
                cur_cache_lines   <= 12'd0;
                cur_partial_drops <= 8'd0;
                state             <= S_CAPTURE;
            end
        end

        S_CAPTURE: begin
            if (!ctrl_enable) begin
                state <= S_IDLE;
            end else if (vsync_rising) begin
                // Frame boundary: latch debug counters, reset for next frame
                dbg_frame_pixels  <= cur_frame_pixels;
                dbg_line_count    <= cur_line_count;
                dbg_cache_lines   <= cur_cache_lines;
                dbg_partial_drops <= cur_partial_drops;
                current_buf       <= ~current_buf;
                byte_cnt          <= 5'd0;
                line_accum        <= 256'd0;
                cur_frame_pixels  <= 17'd0;
                cur_line_count    <= 9'd0;
                cur_cache_lines   <= 12'd0;
                cur_partial_drops <= 8'd0;
                state             <= S_FRAME_DONE;
            end else begin
                // Count lines via HREF rising edge
                if (href_rising)
                    cur_line_count <= cur_line_count + 1'b1;

                // Accumulate accepted Y pixels into cache line
                if (pixel_accept) begin
                    cur_frame_pixels <= cur_frame_pixels + 1'b1;
                    line_accum       <= {pixel_byte, line_accum[255:8]};
                    if (byte_cnt == 5'd31) begin
                        // Cache line complete: snapshot to line_data
                        line_data       <= {pixel_byte, line_accum[255:8]};
                        line_ready      <= 1'b1;
                        cur_cache_lines <= cur_cache_lines + 1'b1;
                        byte_cnt        <= 5'd0;
                        line_accum      <= 256'd0;
                    end else begin
                        byte_cnt <= byte_cnt + 1'b1;
                    end
                end
            end
        end

        S_FRAME_DONE: begin
            frame_done <= 1'b1;
            state      <= S_CAPTURE;
        end

        default: state <= S_IDLE;
        endcase
    end
end

endmodule
