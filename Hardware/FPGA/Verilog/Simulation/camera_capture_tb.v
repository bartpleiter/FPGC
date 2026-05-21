/*
 * CameraCapture V2 testbench
 * ---------------------------
 * Simulates OV7670 QVGA YUV422 timing and verifies:
 *   1. Exactly 2400 cache lines per frame (320 pixels × 240 lines / 32 bytes)
 *   2. Exactly 76800 Y pixels per frame (reported by dbg_frame_pixels)
 *   3. Exactly 240 lines per frame (reported by dbg_line_count)
 *   4. Zero partial cache line drops (320 pixels = 10 × 32, always aligned)
 *   5. line_ready/line_ack handshake works
 *   6. frame_done fires once per VSYNC
 *   7. current_buf toggles each frame
 *   8. Line pixel counter caps at 320 (extra HREF bytes are ignored)
 *   9. Partial cache lines at line boundaries are discarded
 */
`timescale 1ns / 1ps

module camera_capture_tb;

    reg clk = 0;
    always #5 clk = ~clk;  // 100 MHz

    reg cam_pclk = 0;
    always #20 cam_pclk = ~cam_pclk;  // ~25 MHz

    reg         reset = 1;
    reg         cam_vsync = 1;
    reg         cam_href  = 0;
    reg  [7:0]  cam_data  = 8'd0;

    wire [255:0] line_data;
    wire         line_ready;
    reg          line_ack = 0;

    reg          ctrl_enable = 0;

    wire         frame_done;
    wire         current_buf;
    wire [2:0]   dbg_state;
    wire [16:0]  dbg_frame_pixels;
    wire [8:0]   dbg_line_count;
    wire [11:0]  dbg_cache_lines;
    wire [7:0]   dbg_partial_drops;

    CameraCapture uut (
        .clk(clk),
        .reset(reset),
        .cam_pclk(cam_pclk),
        .cam_vsync(cam_vsync),
        .cam_href(cam_href),
        .cam_data(cam_data),
        .line_data(line_data),
        .line_ready(line_ready),
        .line_ack(line_ack),
        .ctrl_enable(ctrl_enable),
        .ctrl_byte_phase(1'b0),
        .frame_done(frame_done),
        .current_buf(current_buf),
        .dbg_state(dbg_state),
        .dbg_frame_pixels(dbg_frame_pixels),
        .dbg_line_count(dbg_line_count),
        .dbg_cache_lines(dbg_cache_lines),
        .dbg_partial_drops(dbg_partial_drops)
    );

    // --- DMA-like consumer: ack 1 cycle after line_ready ---
    integer line_count = 0;
    integer frame_done_count = 0;

    always @(posedge clk) begin
        line_ack <= 0;
        if (line_ready && !line_ack) begin
            line_ack <= 1;
            line_count <= line_count + 1;
        end
    end

    always @(posedge clk) begin
        if (frame_done)
            frame_done_count <= frame_done_count + 1;
    end

    // --- OV7670 stimulus: standard QVGA YUV422 frame ---
    // 240 lines, 640 bytes per line (320 pixel pairs × 2 bytes each)
    // Byte order: U0, Y0, V0, Y1, U2, Y2, V2, Y3, ...
    task generate_frame;
        input integer frame_num;
        input integer extra_href_bytes;  // Extra bytes per line beyond 640 (test pixel cap)
        integer line, byte_idx;
        begin
            @(posedge cam_pclk);
            cam_vsync <= 0;
            repeat (10) @(posedge cam_pclk);  // Active frame starts

            for (line = 0; line < 240; line = line + 1) begin
                cam_href <= 1;
                // Standard 640 bytes (produces 320 Y pixels)
                for (byte_idx = 0; byte_idx < 640; byte_idx = byte_idx + 1) begin
                    if (byte_idx[0] == 1)
                        cam_data <= (line + byte_idx / 2 + frame_num) & 8'hFF;  // Y byte
                    else
                        cam_data <= 8'h80;  // U/V byte (discarded)
                    @(posedge cam_pclk);
                end
                // Optional extra bytes (should be ignored by pixel cap at 320)
                if (extra_href_bytes > 0) begin
                    for (byte_idx = 0; byte_idx < extra_href_bytes; byte_idx = byte_idx + 1) begin
                        cam_data <= 8'hEE;  // Garbage byte — should be ignored
                        @(posedge cam_pclk);
                    end
                end
                cam_href <= 0;
                cam_data <= 8'd0;
                repeat (40) @(posedge cam_pclk);  // Horizontal blanking
            end

            cam_vsync <= 1;
            repeat (1000) @(posedge cam_pclk);  // Vertical blanking
        end
    endtask

    // --- Generate a frame with non-aligned line width (test partial drop) ---
    // Each line has only 640-2 = 638 bytes → 319 Y pixels → 9×32 + 31 = partial
    task generate_frame_partial;
        input integer frame_num;
        integer line, byte_idx;
        begin
            @(posedge cam_pclk);
            cam_vsync <= 0;
            repeat (10) @(posedge cam_pclk);

            for (line = 0; line < 240; line = line + 1) begin
                cam_href <= 1;
                // 638 bytes → 319 Y pixels → 9 full cache lines (288 pixels) + 31 partial
                for (byte_idx = 0; byte_idx < 638; byte_idx = byte_idx + 1) begin
                    if (byte_idx[0] == 1)
                        cam_data <= (line + byte_idx / 2 + frame_num) & 8'hFF;
                    else
                        cam_data <= 8'h80;
                    @(posedge cam_pclk);
                end
                cam_href <= 0;
                cam_data <= 8'd0;
                repeat (40) @(posedge cam_pclk);
            end

            cam_vsync <= 1;
            repeat (1000) @(posedge cam_pclk);
        end
    endtask

    // --- Test sequence ---
    integer errors = 0;

    initial begin
        #100;
        reset <= 0;
        #100;

        ctrl_enable <= 1;
        #100;

        // Initial VSYNC pulse to enter S_CAPTURE
        @(posedge cam_pclk); cam_vsync <= 0;
        repeat(10) @(posedge cam_pclk);
        cam_vsync <= 1;
        repeat(1000) @(posedge cam_pclk);

        $display("=== CameraCapture V2 Testbench ===");

        // ---- Test 1: Standard QVGA frame ----
        $display("");
        $display("--- Test 1: Standard QVGA frame (320x240) ---");
        line_count = 0;
        frame_done_count = 0;
        generate_frame(0, 0);
        repeat (5000) @(posedge clk);

        $display("  DMA line_count    = %0d (expect 2400)", line_count);
        $display("  frame_done_count  = %0d (expect 1)", frame_done_count);
        $display("  current_buf       = %0d (expect 1)", current_buf);
        $display("  dbg_frame_pixels  = %0d (expect 76800)", dbg_frame_pixels);
        $display("  dbg_line_count    = %0d (expect 240)", dbg_line_count);
        $display("  dbg_cache_lines   = %0d (expect 2400)", dbg_cache_lines);
        $display("  dbg_partial_drops = %0d (expect 0)", dbg_partial_drops);

        if (line_count != 2400) begin
            $display("  FAIL: DMA line count");
            errors = errors + 1;
        end
        if (frame_done_count != 1) begin
            $display("  FAIL: frame_done count");
            errors = errors + 1;
        end
        if (current_buf != 1) begin
            $display("  FAIL: current_buf");
            errors = errors + 1;
        end
        if (dbg_frame_pixels != 76800) begin
            $display("  FAIL: dbg_frame_pixels");
            errors = errors + 1;
        end
        if (dbg_line_count != 240) begin
            $display("  FAIL: dbg_line_count");
            errors = errors + 1;
        end
        if (dbg_cache_lines != 2400) begin
            $display("  FAIL: dbg_cache_lines");
            errors = errors + 1;
        end
        if (dbg_partial_drops != 0) begin
            $display("  FAIL: dbg_partial_drops");
            errors = errors + 1;
        end

        // ---- Test 2: Second frame (current_buf toggle) ----
        $display("");
        $display("--- Test 2: Second frame (current_buf toggle) ---");
        line_count = 0;
        frame_done_count = 0;
        generate_frame(1, 0);
        repeat (5000) @(posedge clk);

        $display("  DMA line_count = %0d (expect 2400)", line_count);
        $display("  current_buf    = %0d (expect 0)", current_buf);

        if (line_count != 2400) begin
            $display("  FAIL: DMA line count");
            errors = errors + 1;
        end
        if (current_buf != 0) begin
            $display("  FAIL: current_buf");
            errors = errors + 1;
        end

        // ---- Test 3: Extra HREF bytes (pixel cap at 320) ----
        $display("");
        $display("--- Test 3: Extra HREF bytes (pixel cap at 320) ---");
        line_count = 0;
        frame_done_count = 0;
        generate_frame(2, 20);  // 20 extra bytes per line
        repeat (5000) @(posedge clk);

        $display("  DMA line_count    = %0d (expect 2400)", line_count);
        $display("  dbg_frame_pixels  = %0d (expect 76800)", dbg_frame_pixels);
        $display("  dbg_partial_drops = %0d (expect 0)", dbg_partial_drops);

        if (line_count != 2400) begin
            $display("  FAIL: DMA line count with extra bytes");
            errors = errors + 1;
        end
        if (dbg_frame_pixels != 76800) begin
            $display("  FAIL: pixel cap did not limit to 320 per line");
            errors = errors + 1;
        end
        if (dbg_partial_drops != 0) begin
            $display("  FAIL: dbg_partial_drops");
            errors = errors + 1;
        end

        // ---- Test 4: Partial cache lines (319 pixels per line) ----
        $display("");
        $display("--- Test 4: Partial cache lines (319 Y pixels per line) ---");
        line_count = 0;
        frame_done_count = 0;
        generate_frame_partial(3);
        repeat (5000) @(posedge clk);

        // 319 pixels / 32 = 9 full cache lines + 31-byte partial (dropped)
        // 9 × 240 = 2160 cache lines, 240 partial drops
        $display("  DMA line_count    = %0d (expect 2160)", line_count);
        $display("  dbg_frame_pixels  = %0d (expect 76560)", dbg_frame_pixels);
        $display("  dbg_line_count    = %0d (expect 240)", dbg_line_count);
        $display("  dbg_cache_lines   = %0d (expect 2160)", dbg_cache_lines);
        $display("  dbg_partial_drops = %0d (expect 240)", dbg_partial_drops);

        if (line_count != 2160) begin
            $display("  FAIL: DMA line count");
            errors = errors + 1;
        end
        if (dbg_frame_pixels != 76560) begin
            $display("  FAIL: dbg_frame_pixels (319*240=76560)");
            errors = errors + 1;
        end
        if (dbg_line_count != 240) begin
            $display("  FAIL: dbg_line_count");
            errors = errors + 1;
        end
        if (dbg_cache_lines != 2160) begin
            $display("  FAIL: dbg_cache_lines");
            errors = errors + 1;
        end
        if (dbg_partial_drops != 240) begin
            $display("  FAIL: dbg_partial_drops");
            errors = errors + 1;
        end

        // ---- Summary ----
        $display("");
        if (errors == 0)
            $display("ALL TESTS PASSED");
        else
            $display("FAILED: %0d errors", errors);

        $finish;
    end

    initial begin
        #800_000_000;
        $display("TIMEOUT");
        $finish;
    end

endmodule
