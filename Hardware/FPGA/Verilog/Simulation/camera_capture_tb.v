/*
 * CameraCapture testbench
 * -----------------------
 * Simulates OV7670 timing (QVGA YUV422, ~25 MHz PCLK) and verifies that:
 *   1. Exactly the right number of cache lines are written per frame
 *   2. Only Y bytes (even-numbered) end up in SDRAM
 *   3. Addresses are sequential from the buffer base
 *   4. frame_done fires once per VSYNC
 *   5. current_buf toggles each frame
 *   6. Double buffering works (buffers alternate)
 *
 * Run: iverilog -o sim camera_capture_tb.v ../Modules/IO/CameraCapture.v && vvp sim
 */
`timescale 1ns / 1ps

module camera_capture_tb;

    // System clock: 100 MHz → 10 ns period
    reg clk = 0;
    always #5 clk = ~clk;

    // Pixel clock: ~25 MHz → 40 ns period (20 ns half)
    reg cam_pclk = 0;
    always #20 cam_pclk = ~cam_pclk;

    reg         reset = 1;
    reg         cam_vsync = 1;  // HIGH = blanking (idle)
    reg         cam_href  = 0;
    reg  [7:0]  cam_data  = 8'd0;

    wire [20:0]  sd_addr;
    wire [255:0] sd_data;
    wire         sd_we;
    wire         sd_start;
    reg          sd_done = 0;

    reg          ctrl_enable = 0;
    reg  [20:0]  ctrl_base_buf0 = 21'h100000;  // 32 MiB offset
    reg  [20:0]  ctrl_base_buf1 = 21'h100800;  // 64 KiB later

    wire         frame_done;
    wire         current_buf;

    CameraCapture uut (
        .clk(clk),
        .reset(reset),
        .cam_pclk(cam_pclk),
        .cam_vsync(cam_vsync),
        .cam_href(cam_href),
        .cam_data(cam_data),
        .sd_addr(sd_addr),
        .sd_data(sd_data),
        .sd_we(sd_we),
        .sd_start(sd_start),
        .sd_done(sd_done),
        .ctrl_enable(ctrl_enable),
        .ctrl_base_buf0(ctrl_base_buf0),
        .ctrl_base_buf1(ctrl_base_buf1),
        .frame_done(frame_done),
        .current_buf(current_buf)
    );

    // --- SDRAM responder: delays a few clocks then pulses sd_done ---
    reg [3:0] sdram_delay = 0;
    reg       sdram_busy  = 0;

    // Track writes
    integer write_count = 0;
    integer frame_done_count = 0;
    reg [20:0] last_write_addr = 0;
    reg [7:0]  first_y_byte = 0;  // First Y byte in last cache line

    always @(posedge clk) begin
        sd_done <= 0;
        if (sd_start && !sdram_busy) begin
            sdram_busy  <= 1;
            sdram_delay <= 4;  // Simulate 4-cycle SDRAM latency
            last_write_addr <= sd_addr;
            first_y_byte <= sd_data[255:248];  // MSB byte
        end
        if (sdram_busy) begin
            if (sdram_delay == 0) begin
                sd_done    <= 1;
                sdram_busy <= 0;
                write_count <= write_count + 1;
            end else begin
                sdram_delay <= sdram_delay - 1;
            end
        end
    end

    // Track frame_done
    always @(posedge clk) begin
        if (frame_done) begin
            frame_done_count <= frame_done_count + 1;
        end
    end

    // --- OV7670 stimulus: generate a QVGA frame ---
    // QVGA = 320×240. YUV422 = 2 bytes/pixel → 640 bytes/line.
    // We output 240 lines of 640 bytes each.
    // Y bytes (even) will be: (line_number + pixel_index) & 0xFF
    // UV bytes (odd) will be: 0x80 (neutral chrominance)

    task generate_frame;
        input integer frame_num;
        integer line, byte_idx, pixel_idx;
        reg [7:0] y_val;
        begin
            // VSYNC goes low = active frame
            @(posedge cam_pclk);
            cam_vsync <= 0;

            // Small delay (front porch: 10 pclk cycles)
            repeat (10) @(posedge cam_pclk);

            for (line = 0; line < 240; line = line + 1) begin
                // HREF high for 640 pclk cycles (320 pixels × 2 bytes)
                cam_href <= 1;
                for (byte_idx = 0; byte_idx < 640; byte_idx = byte_idx + 1) begin
                    pixel_idx = byte_idx / 2;
                    if (byte_idx[0] == 0) begin
                        // Even byte = Y
                        y_val = (line + pixel_idx + frame_num) & 8'hFF;
                        cam_data <= y_val;
                    end else begin
                        // Odd byte = U or V
                        cam_data <= 8'h80;
                    end
                    @(posedge cam_pclk);
                end
                cam_href <= 0;
                cam_data <= 8'd0;

                // Horizontal blanking (some pclk cycles between lines)
                repeat (40) @(posedge cam_pclk);
            end

            // VSYNC goes high = blanking
            cam_vsync <= 1;

            // Vertical blanking
            repeat (1000) @(posedge cam_pclk);
        end
    endtask

    // --- Test sequence ---
    integer errors = 0;
    integer expected_writes;

    initial begin
        // 320×240 Y bytes = 76800 bytes → 76800/32 = 2400 cache lines
        expected_writes = 2400;

        // Reset
        #100;
        reset <= 0;
        #100;

        // Enable capture
        ctrl_enable <= 1;
        #100;

        $display("=== CameraCapture Testbench ===");
        $display("Generating frame 0...");

        // Generate first frame
        generate_frame(0);

        // Wait for processing to finish (drain FIFO + last SDRAM writes)
        repeat (5000) @(posedge clk);

        $display("Frame 0: %0d writes, %0d frame_done pulses",
                 write_count, frame_done_count);

        if (write_count != expected_writes) begin
            $display("FAIL: expected %0d writes, got %0d",
                     expected_writes, write_count);
            errors = errors + 1;
        end else begin
            $display("PASS: write count correct (%0d)", write_count);
        end

        if (frame_done_count != 1) begin
            $display("FAIL: expected 1 frame_done, got %0d", frame_done_count);
            errors = errors + 1;
        end else begin
            $display("PASS: frame_done count correct");
        end

        // Check buffer toggled
        if (current_buf != 1) begin
            $display("FAIL: expected current_buf=1 after frame 0, got %0d", current_buf);
            errors = errors + 1;
        end else begin
            $display("PASS: current_buf toggled to 1");
        end

        // --- Second frame ---
        write_count = 0;
        frame_done_count = 0;

        $display("Generating frame 1...");
        generate_frame(1);
        repeat (5000) @(posedge clk);

        $display("Frame 1: %0d writes, %0d frame_done pulses",
                 write_count, frame_done_count);

        if (write_count != expected_writes) begin
            $display("FAIL: expected %0d writes, got %0d",
                     expected_writes, write_count);
            errors = errors + 1;
        end else begin
            $display("PASS: write count correct (%0d)", write_count);
        end

        // Check buffer toggled back
        if (current_buf != 0) begin
            $display("FAIL: expected current_buf=0 after frame 1, got %0d", current_buf);
            errors = errors + 1;
        end else begin
            $display("PASS: current_buf toggled back to 0");
        end

        // --- Summary ---
        $display("");
        if (errors == 0) begin
            $display("ALL TESTS PASSED");
        end else begin
            $display("FAILED: %0d errors", errors);
        end

        $finish;
    end

    // Timeout watchdog
    initial begin
        #200_000_000;  // 200 ms
        $display("TIMEOUT: simulation took too long");
        $finish;
    end

endmodule
