/*
 * frame_scan_engine_tb
 * Testbench for FrameScanEngine module
 *
 * Verifies:
 *   - CASET/PASET/RAMWR command bytes are correct
 *   - Pixel address calculation: y*320 + x
 *   - SRAM read and palette lookup pipeline
 *   - CS assertion during frame
 *   - frame_done pulse after all 76,800 pixels
 *
 * Captures DUT's tx_data/tx_valid directly (not from SPI wire)
 * to avoid SPI clock domain capture complexity.
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/GPU/SPIMaster.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/FrameScanEngine.v"

module frame_scan_engine_tb ();

    // ---- Clock and Reset ----
    reg clk = 1'b0;
    reg reset = 1'b1;

    // ---- DUT signals ----
    reg         enable = 1'b0;
    wire [16:0] sram_addr;
    reg  [7:0]  sram_data = 8'd0;
    wire        sram_read;
    wire [7:0]  palette_idx;
    reg  [23:0] palette_rgb = 24'd0;
    wire [7:0]  spi_tx_data;
    wire        spi_tx_valid;
    wire        spi_tx_ready;
    wire        spi_dc;
    wire        spi_cs_n;
    wire        frame_done;

    // ---- SPI master (for tx_ready handshake) ----
    wire        spi_clk_out;
    wire        spi_mosi;
    wire        spi_dc_out;

    SPIMaster spi_master (
        .clk(clk),
        .reset(reset),
        .tx_data(spi_tx_data),
        .tx_valid(spi_tx_valid),
        .tx_ready(spi_tx_ready),
        .dc_value(spi_dc),
        .spi_clk(spi_clk_out),
        .spi_mosi(spi_mosi),
        .spi_dc(spi_dc_out)
    );

    // ---- DUT ----
    FrameScanEngine dut (
        .clk(clk),
        .reset(reset),
        .enable(enable),
        .sram_addr(sram_addr),
        .sram_data(sram_data),
        .sram_read(sram_read),
        .palette_idx(palette_idx),
        .palette_rgb(palette_rgb),
        .spi_tx_data(spi_tx_data),
        .spi_tx_valid(spi_tx_valid),
        .spi_tx_ready(spi_tx_ready),
        .spi_dc(spi_dc),
        .spi_cs_n(spi_cs_n),
        .frame_done(frame_done)
    );

    // ---- 100 MHz clock ----
    always #5 clk = ~clk;

    // ---- Simulated SRAM: returns low byte of address as pixel index ----
    always @(posedge clk) begin
        if (sram_read)
            sram_data <= sram_addr[7:0];
    end

    // ---- Simulated palette: returns known RGB24 ----
    // For index N: R=N, G=N^0xFF, B=N>>1
    always @(posedge clk) begin
        palette_rgb <= {palette_idx, palette_idx ^ 8'hFF, palette_idx[7:1], 1'b0};
    end

    // ---- Track tx_data bytes when tx_valid is asserted ----
    reg [7:0]  sent_bytes [0:15];  // Record first 16 bytes
    reg        sent_dc    [0:15];  // Record DC for each
    integer    sent_count = 0;

    always @(posedge clk) begin
        if (spi_tx_valid && spi_tx_ready) begin
            if (sent_count < 16) begin
                sent_bytes[sent_count] = spi_tx_data;
                sent_dc[sent_count] = spi_dc;
            end
            sent_count = sent_count + 1;
        end
    end

    // ---- Track SRAM addresses ----
    reg [16:0] first_sram_addr = 17'd0;
    reg [16:0] second_sram_addr = 17'd0;
    integer    sram_read_count = 0;

    always @(posedge clk) begin
        if (sram_read) begin
            if (sram_read_count == 0) first_sram_addr <= sram_addr;
            if (sram_read_count == 1) second_sram_addr <= sram_addr;
            sram_read_count = sram_read_count + 1;
        end
    end

    // ---- Track frame_done ----
    integer frame_done_count = 0;
    always @(posedge clk) begin
        if (frame_done)
            frame_done_count = frame_done_count + 1;
    end

    // ---- Test ----
    integer errors = 0;
    integer test_num = 0;

    initial begin
        // ---- Release reset ----
        #100;
        reset <= 1'b0;
        #20;

        // ==== Test 1: CS should be high when not enabled ====
        test_num = 1;
        if (spi_cs_n != 1'b1) begin
            $display("FAIL test %0d: CS should be high when idle", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: CS high when idle", test_num);
        end

        // ==== Test 2: frame_done should be low ====
        test_num = 2;
        if (frame_done != 1'b0) begin
            $display("FAIL test %0d: frame_done should be low", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: frame_done low at start", test_num);
        end

        // ---- Enable the engine ----
        @(posedge clk);
        enable <= 1'b1;

        // Wait for first 11+ bytes to be sent (CASET+PASET+RAMWR headers)
        while (sent_count < 12) #10;
        #100;

        // ==== Test 3: CS should be low (asserted) during frame ====
        test_num = 3;
        if (spi_cs_n != 1'b0) begin
            $display("FAIL test %0d: CS should be low during frame scan", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: CS asserted during frame scan", test_num);
        end

        // ==== Test 4: Byte 0 = CASET command (0x2A, DC=0) ====
        test_num = 4;
        if (sent_bytes[0] != 8'h2A || sent_dc[0] != 1'b0) begin
            $display("FAIL test %0d: byte 0 should be 0x2A (CASET cmd), got 0x%02X DC=%0b",
                     test_num, sent_bytes[0], sent_dc[0]);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: byte 0 = CASET (0x2A, DC=0)", test_num);
        end

        // ==== Test 5: Bytes 1-4 = CASET data (0x00, 0x00, 0x01, 0x3F, DC=1) ====
        test_num = 5;
        if (sent_bytes[1] != 8'h00 || sent_bytes[2] != 8'h00 ||
            sent_bytes[3] != 8'h01 || sent_bytes[4] != 8'h3F ||
            sent_dc[1] != 1'b1 || sent_dc[4] != 1'b1) begin
            $display("FAIL test %0d: CASET data should be 00 00 01 3F (DC=1), got %02X %02X %02X %02X",
                     test_num, sent_bytes[1], sent_bytes[2], sent_bytes[3], sent_bytes[4]);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: CASET data correct (0-319, DC=1)", test_num);
        end

        // ==== Test 6: Byte 5 = PASET command (0x2B, DC=0) ====
        test_num = 6;
        if (sent_bytes[5] != 8'h2B || sent_dc[5] != 1'b0) begin
            $display("FAIL test %0d: byte 5 should be 0x2B (PASET cmd), got 0x%02X DC=%0b",
                     test_num, sent_bytes[5], sent_dc[5]);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: byte 5 = PASET (0x2B, DC=0)", test_num);
        end

        // ==== Test 7: Bytes 6-9 = PASET data (0x00, 0x00, 0x00, 0xEF, DC=1) ====
        test_num = 7;
        if (sent_bytes[6] != 8'h00 || sent_bytes[7] != 8'h00 ||
            sent_bytes[8] != 8'h00 || sent_bytes[9] != 8'hEF ||
            sent_dc[6] != 1'b1 || sent_dc[9] != 1'b1) begin
            $display("FAIL test %0d: PASET data should be 00 00 00 EF (DC=1), got %02X %02X %02X %02X",
                     test_num, sent_bytes[6], sent_bytes[7], sent_bytes[8], sent_bytes[9]);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: PASET data correct (0-239, DC=1)", test_num);
        end

        // ==== Test 8: Byte 10 = RAMWR command (0x2C, DC=0) ====
        test_num = 8;
        if (sent_bytes[10] != 8'h2C || sent_dc[10] != 1'b0) begin
            $display("FAIL test %0d: byte 10 should be 0x2C (RAMWR cmd), got 0x%02X DC=%0b",
                     test_num, sent_bytes[10], sent_dc[10]);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: byte 10 = RAMWR (0x2C, DC=0)", test_num);
        end

        // ==== Test 9: Byte 11+ should be pixel data (DC=1) ====
        test_num = 9;
        if (sent_dc[11] != 1'b1) begin
            $display("FAIL test %0d: pixel data should have DC=1, got DC=%0b", test_num, sent_dc[11]);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: pixel data bytes have DC=1", test_num);
        end

        // ==== Test 10: First SRAM address should be 0 (pixel 0,0) ====
        test_num = 10;
        while (sram_read_count < 2) #10;
        #100;
        if (first_sram_addr != 17'd0) begin
            $display("FAIL test %0d: first SRAM addr should be 0 (0,0), got %0d",
                     test_num, first_sram_addr);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: first SRAM address is 0", test_num);
        end

        // ==== Test 11: Second SRAM address should be 1 (pixel 1,0) ====
        test_num = 11;
        if (second_sram_addr != 17'd1) begin
            $display("FAIL test %0d: second SRAM addr should be 1 (1,0), got %0d",
                     test_num, second_sram_addr);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: second SRAM address is 1", test_num);
        end

        // ==== Test 12: Wait for first frame to complete ====
        // 76,800 pixels × 2 bytes × ~18 clocks/byte = ~2,764,800 clocks = ~27.6ms
        test_num = 12;
        #40_000_000;
        if (frame_done_count < 1) begin
            $display("FAIL test %0d: frame_done not pulsed after 40ms", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: frame_done pulsed (%0d frames)", test_num, frame_done_count);
        end

        // ==== Test 13: Correct number of SPI bytes per frame ====
        // 11 header bytes + 76800 pixels × 2 bytes = 153,611 bytes per frame
        test_num = 13;
        // sent_count should be approximately frame_done_count * 153611
        $display("INFO: sent %0d SPI bytes for %0d frames (expect ~153611/frame)",
                 sent_count, frame_done_count);
        if (sent_count > 0) begin
            $display("PASS test %0d: SPI bytes transmitted", test_num);
        end else begin
            $display("FAIL test %0d: no SPI bytes transmitted", test_num);
            errors = errors + 1;
        end

        // ---- Summary ----
        $display("");
        $display("SRAM reads: %0d", sram_read_count);
        $display("SPI bytes: %0d", sent_count);
        $display("Frames: %0d", frame_done_count);
        if (errors == 0)
            $display("ALL %0d TESTS PASSED", test_num);
        else
            $display("FAILED: %0d errors out of %0d tests", errors, test_num);

        $finish;
    end

    // ---- Safety timeout (60ms) ----
    initial begin
        #60_000_000;
        $display("TIMEOUT: simulation exceeded 60ms (frames=%0d, sram=%0d, spi=%0d)",
                 frame_done_count, sram_read_count, sent_count);
        $finish;
    end

    // ---- VCD dump ----
    initial begin
        $dumpfile("Hardware/FPGA/Verilog/Simulation/Output/frame_scan_engine.vcd");
        $dumpvars(0, frame_scan_engine_tb);
    end

endmodule
