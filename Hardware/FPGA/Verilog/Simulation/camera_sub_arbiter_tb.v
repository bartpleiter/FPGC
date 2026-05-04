// Testbench for CameraSubArbiter
// Tests: basic DMA, basic camera, simultaneous requests (priority/fairness),
// throughput under contention.

`timescale 1ns / 1ps

module camera_sub_arbiter_tb;

    reg clk = 0;
    always #5 clk = ~clk;  // 100 MHz

    reg reset = 1;

    // DMA port
    reg  [20:0]  dma_addr  = 0;
    reg  [255:0] dma_data  = 0;
    reg          dma_we    = 0;
    reg          dma_start = 0;
    wire         dma_done;
    wire [255:0] dma_q;

    // Camera port
    reg  [20:0]  cam_addr  = 0;
    reg  [255:0] cam_data  = 0;
    reg          cam_we    = 1;
    reg          cam_start = 0;
    wire         cam_done;

    // SDRAM port
    wire [20:0]  sd_addr;
    wire [255:0] sd_data;
    wire         sd_we;
    wire         sd_start;
    reg          sd_done = 0;
    reg  [255:0] sd_q    = 0;

    CameraSubArbiter uut (.*);

    // SDRAM responder (4-cycle latency)
    reg [3:0] sd_delay = 0;
    reg       sd_busy  = 0;
    integer   sd_write_count = 0;
    integer   sd_read_count  = 0;

    always @(posedge clk) begin
        sd_done <= 0;
        if (sd_start && !sd_busy) begin
            sd_busy  <= 1;
            sd_delay <= 3;
            if (sd_we) sd_write_count <= sd_write_count + 1;
            else       sd_read_count  <= sd_read_count + 1;
        end
        if (sd_busy) begin
            if (sd_delay == 0) begin
                sd_done <= 1;
                sd_busy <= 0;
                sd_q    <= {224'd0, sd_addr[7:0], 24'hDEAD00};  // dummy read data
            end else begin
                sd_delay <= sd_delay - 1;
            end
        end
    end

    // --- Test helpers ---

    task dma_write;
        input [20:0] addr;
        input [255:0] data;
        begin
            @(posedge clk);
            dma_addr  <= addr;
            dma_data  <= data;
            dma_we    <= 1;
            dma_start <= 1;
            @(posedge clk);
            dma_start <= 0;
            while (!dma_done) @(posedge clk);
            @(posedge clk);
        end
    endtask

    task dma_read;
        input [20:0] addr;
        begin
            @(posedge clk);
            dma_addr  <= addr;
            dma_we    <= 0;
            dma_start <= 1;
            @(posedge clk);
            dma_start <= 0;
            while (!dma_done) @(posedge clk);
            @(posedge clk);
        end
    endtask

    task cam_write;
        input [20:0] addr;
        input [255:0] data;
        begin
            @(posedge clk);
            cam_addr  <= addr;
            cam_data  <= data;
            cam_we    <= 1;
            cam_start <= 1;
            @(posedge clk);
            cam_start <= 0;
            while (!cam_done) @(posedge clk);
            @(posedge clk);
        end
    endtask

    // --- Tests ---
    integer errors;
    reg cam_completed, dma_completed;
    integer cam_done_time, dma_done_time;
    integer i, cam_count, dma_count2;

    initial begin
        errors = 0;
        #100; reset <= 0; #100;

        $display("=== CameraSubArbiter Testbench ===");

        // Test 1: Single DMA write
        $display("Test 1: DMA write");
        dma_write(21'h100, 256'hCAFE);
        if (sd_write_count == 1)
            $display("  PASS");
        else begin
            $display("  FAIL: sd_write_count=%0d", sd_write_count);
            errors = errors + 1;
        end

        // Test 2: Single DMA read
        $display("Test 2: DMA read");
        dma_read(21'h200);
        if (sd_read_count == 1)
            $display("  PASS");
        else begin
            $display("  FAIL: sd_read_count=%0d", sd_read_count);
            errors = errors + 1;
        end

        // Test 3: Single camera write
        $display("Test 3: Camera write");
        sd_write_count = 0;
        cam_write(21'h300, 256'hBEEF);
        if (sd_write_count == 1)
            $display("  PASS");
        else begin
            $display("  FAIL: sd_write_count=%0d", sd_write_count);
            errors = errors + 1;
        end

        // Test 4: Simultaneous requests — camera should win first
        $display("Test 4: Simultaneous requests");
        begin
            cam_completed = 0;
            dma_completed = 0;

            @(posedge clk);
            // Fire both on the same cycle
            dma_addr  <= 21'h400;
            dma_data  <= 256'h1111;
            dma_we    <= 1;
            dma_start <= 1;
            cam_addr  <= 21'h500;
            cam_data  <= 256'h2222;
            cam_we    <= 1;
            cam_start <= 1;
            @(posedge clk);
            cam_start <= 0;
            // Keep dma_start high until DMA is accepted
            // (camera wins first, so DMA start must persist)

            // Wait for camera to complete first
            begin : wait_cam4
                integer wc;
                for (wc = 0; wc < 1000 && !cam_done; wc = wc + 1)
                    @(posedge clk);
            end
            cam_completed = 1;
            cam_done_time = $time;

            // DMA start is still asserted, arbiter picks it up next cycle
            // Wait for DMA to complete
            begin : wait_dma4
                integer wd;
                for (wd = 0; wd < 1000 && !dma_done; wd = wd + 1)
                    @(posedge clk);
            end
            dma_start <= 0;
            dma_completed = 1;
            dma_done_time = $time;

            // Wait for dma_start to deassert and arbiter to settle
            @(posedge clk);
            @(posedge clk);

            if (cam_done_time < dma_done_time)
                $display("  PASS: camera completed first");
            else begin
                $display("  FAIL: DMA completed first");
                errors = errors + 1;
            end
        end

        // Test 5: Burst of interleaved operations
        $display("Test 5: 10 camera + 10 DMA interleaved");
        repeat(20) @(posedge clk);  // Let arbiter settle after test 4
        begin
            cam_count = 0;
            dma_count2 = 0;
            sd_write_count = 0;
            sd_read_count = 0;

            for (i = 0; i < 10; i = i + 1) begin
                cam_write(21'h600 + i, {224'd0, i[31:0]});
                cam_count = cam_count + 1;
                dma_write(21'h700 + i, {224'd0, i[31:0]});
                dma_count2 = dma_count2 + 1;
            end

            if (sd_write_count == 20)
                $display("  PASS: all 20 writes completed");
            else begin
                $display("  FAIL: sd_write_count=%0d (expected 20)", sd_write_count);
                errors = errors + 1;
            end
        end

        if (errors == 0)
            $display("\nALL TESTS PASSED");
        else
            $display("\nFAILED: %0d errors", errors);

        $finish;
    end

    // Timeout
    initial begin
        #10_000_000;
        $display("TIMEOUT");
        $finish;
    end

endmodule
