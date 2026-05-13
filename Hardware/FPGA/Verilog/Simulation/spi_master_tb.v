/*
 * spi_master_tb
 * Testbench for SPIMaster module
 *
 * Verifies:
 *   - Byte transmission (correct SPI clock and MOSI output)
 *   - D/C pin latching per byte
 *   - tx_ready handshake (deasserts during transmission)
 *   - Back-to-back byte streaming
 *   - Reset behavior
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/GPU/SPIMaster.v"

module spi_master_tb ();

    // ---- Clock and Reset ----
    reg clk = 1'b0;
    reg reset = 1'b1;

    // ---- DUT signals ----
    reg  [7:0]  tx_data = 8'd0;
    reg         tx_valid = 1'b0;
    wire        tx_ready;
    reg         dc_value = 1'b0;
    wire        spi_clk_out;
    wire        spi_mosi;
    wire        spi_dc;

    // ---- DUT ----
    SPIMaster dut (
        .clk(clk),
        .reset(reset),
        .tx_data(tx_data),
        .tx_valid(tx_valid),
        .tx_ready(tx_ready),
        .dc_value(dc_value),
        .spi_clk(spi_clk_out),
        .spi_mosi(spi_mosi),
        .spi_dc(spi_dc)
    );

    // ---- 100 MHz clock ----
    always #5 clk = ~clk;

    // ---- Capture SPI output ----
    // Build up a byte from SPI rising edges, latch when complete
    reg [7:0]  shift_cap = 8'd0;
    reg [2:0]  cap_cnt = 3'd0;
    reg [7:0]  last_byte = 8'd0;
    integer    total_bytes = 0;

    always @(posedge spi_clk_out) begin
        shift_cap <= {shift_cap[6:0], spi_mosi};
        cap_cnt <= cap_cnt + 3'd1;
        if (cap_cnt == 3'd7) begin
            last_byte <= {shift_cap[6:0], spi_mosi};
            total_bytes <= total_bytes + 1;
        end
    end

    // ---- Test helpers ----
    task send_byte;
        input [7:0] data;
        input       dc;
        begin
            @(posedge clk);
            while (!tx_ready) @(posedge clk);
            tx_data <= data;
            dc_value <= dc;
            tx_valid <= 1'b1;
            @(posedge clk);
            tx_valid <= 1'b0;
        end
    endtask

    task wait_byte_done;
        begin
            // Wait for SPIMaster to finish (tx_ready goes high)
            @(posedge clk);
            while (!tx_ready) @(posedge clk);
            // Wait a few more cycles for SPI capture to settle
            repeat (4) @(posedge clk);
        end
    endtask

    // ---- Test sequence ----
    integer errors = 0;
    integer test_num = 0;

    initial begin
        // Release reset
        #100;
        reset <= 1'b0;
        #20;

        // Test 1: tx_ready is high after reset
        test_num = 1;
        if (!tx_ready) begin
            $display("FAIL test %0d: tx_ready not high after reset", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: tx_ready high after reset", test_num);
        end

        // Test 2: Send 0xA5 as command (DC=0)
        test_num = 2;
        send_byte(8'hA5, 1'b0);
        // tx_ready should go low during transmission
        @(posedge clk);
        if (tx_ready) begin
            $display("FAIL test %0d: tx_ready didn't go low during TX", test_num);
            errors = errors + 1;
        end
        wait_byte_done();
        if (last_byte != 8'hA5) begin
            $display("FAIL test %0d: expected 0xA5, got 0x%02X", test_num, last_byte);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: byte 0xA5 transmitted correctly", test_num);
        end

        // Test 3: DC pin was 0 (command)
        test_num = 3;
        if (spi_dc != 1'b0) begin
            $display("FAIL test %0d: DC should be 0 for command", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: DC=0 for command byte", test_num);
        end

        // Test 4: Send 0x3C as data (DC=1)
        test_num = 4;
        send_byte(8'h3C, 1'b1);
        wait_byte_done();
        if (last_byte != 8'h3C) begin
            $display("FAIL test %0d: expected 0x3C, got 0x%02X", test_num, last_byte);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: byte 0x3C transmitted correctly", test_num);
        end

        // Test 5: DC pin was 1 (data)
        test_num = 5;
        if (spi_dc != 1'b1) begin
            $display("FAIL test %0d: DC should be 1 for data", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: DC=1 for data byte", test_num);
        end

        // Test 6: Send 0xFF
        test_num = 6;
        send_byte(8'hFF, 1'b1);
        wait_byte_done();
        if (last_byte != 8'hFF) begin
            $display("FAIL test %0d: expected 0xFF, got 0x%02X", test_num, last_byte);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: byte 0xFF transmitted correctly", test_num);
        end

        // Test 7: Send 0x00
        test_num = 7;
        send_byte(8'h00, 1'b0);
        wait_byte_done();
        if (last_byte != 8'h00) begin
            $display("FAIL test %0d: expected 0x00, got 0x%02X", test_num, last_byte);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: byte 0x00 transmitted correctly", test_num);
        end

        // Test 8: Back-to-back bytes
        test_num = 8;
        send_byte(8'hDE, 1'b1);
        wait_byte_done();
        if (last_byte != 8'hDE) begin
            $display("FAIL test %0d: back-to-back byte 1 expected 0xDE, got 0x%02X", test_num, last_byte);
            errors = errors + 1;
        end
        send_byte(8'hAD, 1'b1);
        wait_byte_done();
        if (last_byte != 8'hAD) begin
            $display("FAIL test %0d: back-to-back byte 2 expected 0xAD, got 0x%02X", test_num, last_byte);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: back-to-back bytes correct", test_num);
        end

        // Test 9: SPI clock idle low
        test_num = 9;
        #100;
        if (spi_clk_out != 1'b0) begin
            $display("FAIL test %0d: SPI clock not idle low", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: SPI clock idle low (Mode 0)", test_num);
        end

        // Summary
        $display("");
        $display("Total bytes captured: %0d", total_bytes);
        if (errors == 0)
            $display("ALL %0d TESTS PASSED", test_num);
        else
            $display("FAILED: %0d errors out of %0d tests", errors, test_num);

        $finish;
    end

    // VCD dump
    initial begin
        $dumpfile("Hardware/FPGA/Verilog/Simulation/Output/spi_master.vcd");
        $dumpvars(0, spi_master_tb);
    end

endmodule
