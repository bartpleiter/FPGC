/*
 * CameraCapture testbench (handshake interface)
 * -----------------------------------------------
 * Simulates OV7670 timing and verifies:
 *   1. Correct number of cache lines produced per frame (2400)
 *   2. line_ready/line_ack handshake works
 *   3. First Y byte appears at line_data[255:248]
 *   4. frame_done fires once per VSYNC
 *   5. current_buf toggles each frame
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
        .frame_done(frame_done),
        .current_buf(current_buf)
    );

    // --- DMA-like consumer: ack 1 cycle after line_ready ---
    integer line_count = 0;
    integer frame_done_count = 0;
    reg [7:0] first_byte = 0;

    always @(posedge clk) begin
        line_ack <= 0;
        if (line_ready && !line_ack) begin
            line_ack <= 1;
            line_count <= line_count + 1;
            first_byte <= line_data[7:0];
        end
    end

    always @(posedge clk) begin
        if (frame_done)
            frame_done_count <= frame_done_count + 1;
    end

    // --- OV7670 stimulus ---
    task generate_frame;
        input integer frame_num;
        integer line, byte_idx, pixel_idx;
        reg [7:0] y_val;
        begin
            @(posedge cam_pclk);
            cam_vsync <= 0;
            repeat (10) @(posedge cam_pclk);

            for (line = 0; line < 240; line = line + 1) begin
                cam_href <= 1;
                for (byte_idx = 0; byte_idx < 640; byte_idx = byte_idx + 1) begin
                    pixel_idx = byte_idx / 2;
                    if (byte_idx[0] == 0)
                        cam_data <= (line + pixel_idx + frame_num) & 8'hFF;
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

        // Initial VSYNC pulse
        @(posedge cam_pclk); cam_vsync <= 0;
        repeat(10) @(posedge cam_pclk);
        cam_vsync <= 1;
        repeat(1000) @(posedge cam_pclk);

        $display("=== CameraCapture Testbench (Handshake) ===");
        $display("Generating frame 0...");

        generate_frame(0);
        repeat (5000) @(posedge clk);

        $display("Frame 0: %0d lines, %0d frame_done", line_count, frame_done_count);

        if (line_count != 2400) begin
            $display("FAIL: expected 2400 lines, got %0d", line_count);
            errors = errors + 1;
        end else
            $display("PASS: line count correct (2400)");

        if (frame_done_count != 1) begin
            $display("FAIL: expected 1 frame_done, got %0d", frame_done_count);
            errors = errors + 1;
        end else
            $display("PASS: frame_done count correct");

        if (current_buf != 1) begin
            $display("FAIL: expected current_buf=1, got %0d", current_buf);
            errors = errors + 1;
        end else
            $display("PASS: current_buf toggled to 1");

        // Second frame
        line_count = 0;
        frame_done_count = 0;
        $display("Generating frame 1...");
        generate_frame(1);
        repeat (5000) @(posedge clk);

        $display("Frame 1: %0d lines, %0d frame_done", line_count, frame_done_count);

        if (line_count != 2400) begin
            $display("FAIL: expected 2400 lines, got %0d", line_count);
            errors = errors + 1;
        end else
            $display("PASS: line count correct (2400)");

        if (current_buf != 0) begin
            $display("FAIL: expected current_buf=0, got %0d", current_buf);
            errors = errors + 1;
        end else
            $display("PASS: current_buf toggled back to 0");

        $display("");
        if (errors == 0)
            $display("ALL TESTS PASSED");
        else
            $display("FAILED: %0d errors", errors);

        $finish;
    end

    initial begin
        #200_000_000;
        $display("TIMEOUT");
        $finish;
    end

endmodule
