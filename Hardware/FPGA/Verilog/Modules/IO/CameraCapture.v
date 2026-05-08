/*
 * CameraCapture
 * -------------
 * OV7670 pixel capture engine for the FPGC-Camera project.
 *
 * Captures YUV422 data from the OV7670 parallel interface, extracts the
 * Y (luminance) channel, and accumulates 32-byte (256-bit) cache lines.
 * Completed cache lines are exposed via a ready/ack handshake for the
 * DMA engine to consume and write to SDRAM.
 *
 * Single-clock design:
 *   The OV7670 pixel clock (cam_pclk, ~25 MHz) is sampled as a DATA input
 *   in the 100 MHz system clock domain. Rising edges are detected via a
 *   2-stage synchronizer + edge detect.
 *
 * Handshake interface:
 *   - line_ready: asserted when a 256-bit cache line is available
 *   - line_data:  the 256-bit cache line (first pixel at [255:248])
 *   - line_ack:   DMA engine asserts for 1 cycle to consume the line
 *   Pixel capture continues into the next cache line during the handshake.
 *   If DMA is too slow (line_ready still HIGH when next line completes),
 *   the old line is overwritten (dropped).
 *
 * Y channel extraction:
 *   OV7670 in YUV422 mode outputs bytes in order: U0, Y0, V0, Y1, ...
 *   Every odd byte (index 1, 3, 5...) when HREF is high is a Y value.
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
    output reg          frame_done   = 1'b0,  // 1-cycle pulse on VSYNC
    output reg          current_buf  = 1'b0,  // Toggles each frame

    // Debug outputs
    output wire [2:0]   dbg_state,
    output reg  [15:0]  dbg_write_count = 16'd0  // Cache lines produced
);

// =========================================================================
// Input synchronization (all sensor signals → clk domain)
// =========================================================================
reg pclk_s1 = 1'b0, pclk_s2 = 1'b0, pclk_s3 = 1'b0;
wire pclk_rising = pclk_s2 && !pclk_s3;

reg vsync_s1 = 1'b0, vsync_s2 = 1'b0, vsync_prev = 1'b0;
wire vsync_rising = vsync_s2 && !vsync_prev;

reg href_s1 = 1'b0, href_s2 = 1'b0;
reg [7:0] data_s1 = 8'd0, data_s2 = 8'd0;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        pclk_s1    <= 1'b0; pclk_s2    <= 1'b0; pclk_s3   <= 1'b0;
        vsync_s1   <= 1'b0; vsync_s2   <= 1'b0; vsync_prev <= 1'b0;
        href_s1    <= 1'b0; href_s2    <= 1'b0;
        data_s1    <= 8'd0; data_s2    <= 8'd0;
    end else begin
        pclk_s1  <= cam_pclk;
        pclk_s2  <= pclk_s1;
        pclk_s3  <= pclk_s2;

        vsync_s1   <= cam_vsync;
        vsync_s2   <= vsync_s1;
        vsync_prev <= vsync_s2;

        href_s1  <= cam_href;
        href_s2  <= href_s1;

        data_s1  <= cam_data;
        data_s2  <= data_s1;
    end
end

// =========================================================================
// Pixel capture (on detected PCLK rising edge)
// =========================================================================
reg byte_toggle = 1'b0;
reg pixel_valid = 1'b0;
reg [7:0] pixel_byte = 8'd0;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        byte_toggle <= 1'b0;
        pixel_valid <= 1'b0;
        pixel_byte  <= 8'd0;
    end else begin
        pixel_valid <= 1'b0;

        if (pclk_rising && ctrl_enable) begin
            if (href_s2) begin
                if (byte_toggle) begin
                    pixel_byte  <= data_s2;
                    pixel_valid <= 1'b1;
                end
                byte_toggle <= ~byte_toggle;
            end else begin
                byte_toggle <= 1'b0;
            end
        end else if (!ctrl_enable) begin
            byte_toggle <= 1'b0;
        end
    end
end

// =========================================================================
// Main state machine — cache line assembly + handshake
// =========================================================================
localparam S_IDLE       = 3'd0;
localparam S_CAPTURE    = 3'd1;  // Accumulating Y pixels into shift register
localparam S_FRAME_DONE = 3'd2;  // Signal frame completion

reg [2:0]  state     = S_IDLE;
reg [4:0]  byte_cnt  = 5'd0;
reg [255:0] line_accum = 256'd0;  // Shift register for current cache line

assign dbg_state = state;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        state           <= S_IDLE;
        dbg_write_count <= 16'd0;
        frame_done      <= 1'b0;
        current_buf     <= 1'b0;
        byte_cnt        <= 5'd0;
        line_accum      <= 256'd0;
        line_data       <= 256'd0;
        line_ready      <= 1'b0;
    end else begin
        frame_done <= 1'b0;

        // DMA consumed the cache line
        if (line_ack)
            line_ready <= 1'b0;

        case (state)
        S_IDLE: begin
            if (ctrl_enable && vsync_rising) begin
                current_buf <= 1'b0;
                byte_cnt    <= 5'd0;
                line_accum  <= 256'd0;
                state       <= S_CAPTURE;
            end
        end

        S_CAPTURE: begin
            if (!ctrl_enable) begin
                state <= S_IDLE;
            end else if (vsync_rising) begin
                current_buf <= ~current_buf;
                byte_cnt    <= 5'd0;
                line_accum  <= 256'd0;
                state       <= S_FRAME_DONE;
            end else if (pixel_valid) begin
                line_accum <= {pixel_byte, line_accum[255:8]};
                if (byte_cnt == 5'd31) begin
                    // Cache line complete: snapshot to line_data
                    line_data       <= {pixel_byte, line_accum[255:8]};
                    line_ready      <= 1'b1;
                    dbg_write_count <= dbg_write_count + 1'b1;
                    byte_cnt        <= 5'd0;
                    line_accum      <= 256'd0;
                end else begin
                    byte_cnt <= byte_cnt + 1'b1;
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
