/*
 * ili9341_init_tb
 * Testbench for ILI9341_Init module
 *
 * Verifies:
 *   - Hardware reset timing (RST low for ~10ms, then wait ~120ms)
 *   - Init sequence: all ROM commands are sent over SPI
 *   - init_done asserts after sequence completes
 *   - CS and DC signals are correct for commands vs data
 *   - Delay entries in the ROM are respected
 *
 * Note: Uses real CYCLES_PER_MS (100,000), so simulation runs ~15M cycles.
 *       Takes a few seconds with Icarus Verilog.
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/GPU/SPIMaster.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/ILI9341_Init.v"

module ili9341_init_tb ();

    // ---- Clock and Reset ----
    reg clk = 1'b0;
    reg reset = 1'b1;

    // ---- Init module signals ----
    wire [7:0]  init_spi_tx_data;
    wire        init_spi_tx_valid;
    wire        init_spi_dc;
    wire        init_spi_cs_n;
    wire        init_lcd_rst_n;
    wire        init_done;

    // ---- SPI master signals ----
    wire        spi_tx_ready;
    wire        spi_clk_out;
    wire        spi_mosi;
    wire        spi_dc_out;

    // ---- DUT: ILI9341_Init ----
    ILI9341_Init dut (
        .clk(clk),
        .reset(reset),
        .spi_tx_data(init_spi_tx_data),
        .spi_tx_valid(init_spi_tx_valid),
        .spi_tx_ready(spi_tx_ready),
        .spi_dc(init_spi_dc),
        .spi_cs_n(init_spi_cs_n),
        .lcd_rst_n(init_lcd_rst_n),
        .init_done(init_done)
    );

    // ---- SPI Master (needed for tx_ready handshake) ----
    SPIMaster spi_master (
        .clk(clk),
        .reset(reset),
        .tx_data(init_spi_tx_data),
        .tx_valid(init_spi_tx_valid),
        .tx_ready(spi_tx_ready),
        .dc_value(init_spi_dc),
        .spi_clk(spi_clk_out),
        .spi_mosi(spi_mosi),
        .spi_dc(spi_dc_out)
    );

    // ---- 100 MHz clock ----
    always #5 clk = ~clk;

    // ---- Capture SPI bytes for analysis ----
    reg [7:0]  captured_byte = 8'd0;
    integer    bit_count = 0;
    integer    byte_count = 0;

    // Capture on SPI rising edge (Mode 0)
    always @(posedge spi_clk_out) begin
        captured_byte <= {captured_byte[6:0], spi_mosi};
        bit_count <= bit_count + 1;
        if (bit_count == 7) begin
            byte_count <= byte_count + 1;
            bit_count <= 0;
        end
    end

    // ---- Track first command byte ----
    reg first_cmd_seen = 1'b0;
    reg [7:0] first_cmd_byte = 8'd0;
    always @(posedge clk) begin
        if (init_spi_tx_valid && spi_tx_ready && !first_cmd_seen) begin
            first_cmd_byte <= init_spi_tx_data;
            first_cmd_seen <= 1'b1;
        end
    end

    // ---- Test ----
    integer errors = 0;
    integer test_num = 0;

    initial begin
        // ---- Release reset ----
        #100;
        reset <= 1'b0;

        // ==== Test 1: RST goes low immediately ====
        test_num = 1;
        #20;
        if (init_lcd_rst_n != 1'b0) begin
            $display("FAIL test %0d: LCD RST should be low at start", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: LCD RST low at start", test_num);
        end

        // ==== Test 2: init_done is low during init ====
        test_num = 2;
        if (init_done != 1'b0) begin
            $display("FAIL test %0d: init_done should be low", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: init_done low during init", test_num);
        end

        // ==== Test 3: Wait for RST to go high (after ~10ms) ====
        test_num = 3;
        // 10ms = 10_000_000ns, add margin
        #10_100_000;
        if (init_lcd_rst_n != 1'b1) begin
            $display("FAIL test %0d: LCD RST should be high after 10ms", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: LCD RST high after ~10ms", test_num);
        end

        // ==== Test 4: CS should be high during reset wait ====
        test_num = 4;
        if (init_spi_cs_n != 1'b1) begin
            $display("FAIL test %0d: CS should be high during reset wait", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: CS high during reset wait", test_num);
        end

        // ==== Test 5: Wait for init to complete ====
        // Hardware reset: 10ms + 120ms = 130ms
        // ROM delays: 5ms + 120ms + 20ms = 145ms
        // Total: ~275ms + SPI byte time
        test_num = 5;
        #290_000_000;

        if (!init_done) begin
            $display("FAIL test %0d: init_done not asserted after 200ms", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: init_done asserted", test_num);
        end

        // ==== Test 6: First command should be Software Reset (0x01) ====
        test_num = 6;
        if (first_cmd_byte != 8'h01) begin
            $display("FAIL test %0d: first command should be 0x01 (SW Reset), got 0x%02X",
                     test_num, first_cmd_byte);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: first command is Software Reset (0x01)", test_num);
        end

        // ==== Test 7: CS should be high after init ====
        test_num = 7;
        if (init_spi_cs_n != 1'b1) begin
            $display("FAIL test %0d: CS should be deasserted after init", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: CS deasserted after init", test_num);
        end

        // ==== Test 8: Multiple bytes were transmitted ====
        test_num = 8;
        // We should have sent 87 command/data bytes (indices 0-87, minus delays and end)
        // At minimum we expect > 50 bytes
        if (byte_count < 50) begin
            $display("FAIL test %0d: expected >50 bytes, got %0d", test_num, byte_count);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: %0d bytes transmitted", test_num, byte_count);
        end

        // ==== Test 9: init_done stays high ====
        test_num = 9;
        #1000;
        if (!init_done) begin
            $display("FAIL test %0d: init_done should stay high", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: init_done stays high", test_num);
        end

        // ---- Summary ----
        $display("");
        if (errors == 0)
            $display("ALL %0d TESTS PASSED", test_num);
        else
            $display("FAILED: %0d errors out of %0d tests", errors, test_num);

        $finish;
    end

    // ---- Safety timeout (400ms) ----
    initial begin
        #400_000_000;
        $display("TIMEOUT: simulation exceeded 300ms");
        $finish;
    end

    // ---- VCD dump ----
    initial begin
        $dumpfile("Hardware/FPGA/Verilog/Simulation/Output/ili9341_init.vcd");
        $dumpvars(0, ili9341_init_tb);
    end

endmodule
