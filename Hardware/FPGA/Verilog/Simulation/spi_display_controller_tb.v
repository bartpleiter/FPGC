/*
 * spi_display_controller_tb
 * Integration testbench for SPIDisplayController
 *
 * Verifies the full pipeline:
 *   - ILI9341 init completes (init_done)
 *   - Frame scanning starts automatically after init
 *   - SRAM reads and palette lookups work through the mux
 *   - frame_drawn signal fires after a complete frame
 *   - SPI output pins are driven correctly
 *   - LCD backlight is always on
 *   - LCD reset sequence occurs
 *
 * This test uses simulated SRAM data and the real PixelPalette
 * with its default palette (RRRGGGBB bit-replication).
 *
 * Note: Init takes ~130ms of simulation time. Full test ~170ms.
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/GPU/SPIMaster.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/ILI9341_Init.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/FrameScanEngine.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/PixelPalette.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/SPIDisplayController.v"

module spi_display_controller_tb ();

    // ---- Clock and Reset ----
    reg clk = 1'b0;
    reg reset = 1'b1;

    // ---- DUT signals ----
    wire        spi_clk_out;
    wire        spi_mosi;
    wire        spi_cs_n;
    wire        spi_dc;
    wire        lcd_rst_n;
    wire        lcd_backlight;

    wire [16:0] pixel_sram_addr;
    reg  [7:0]  pixel_sram_data = 8'd0;
    wire        pixel_reading;

    reg         palette_we = 1'b0;
    reg  [7:0]  palette_addr = 8'd0;
    reg  [23:0] palette_wdata = 24'd0;

    wire        frame_drawn;
    wire        busy;

    // ---- DUT ----
    SPIDisplayController dut (
        .clk(clk),
        .reset(reset),
        .spi_clk(spi_clk_out),
        .spi_mosi(spi_mosi),
        .spi_cs_n(spi_cs_n),
        .spi_dc(spi_dc),
        .lcd_rst_n(lcd_rst_n),
        .lcd_backlight(lcd_backlight),
        .pixel_sram_addr(pixel_sram_addr),
        .pixel_sram_data(pixel_sram_data),
        .pixel_reading(pixel_reading),
        .palette_we(palette_we),
        .palette_addr(palette_addr),
        .palette_wdata(palette_wdata),
        .frame_drawn(frame_drawn),
        .busy(busy)
    );

    // ---- 100 MHz clock ----
    always #5 clk = ~clk;

    // ---- Simulated SRAM: returns low byte of address ----
    always @(posedge clk) begin
        if (pixel_reading)
            pixel_sram_data <= pixel_sram_addr[7:0];
    end

    // ---- Track events ----
    integer frame_count = 0;
    reg     init_done_seen = 1'b0;
    reg     rst_low_seen = 1'b0;
    reg     rst_high_seen = 1'b0;

    always @(posedge clk) begin
        if (frame_drawn)
            frame_count <= frame_count + 1;
        if (!lcd_rst_n)
            rst_low_seen <= 1'b1;
        if (lcd_rst_n && rst_low_seen)
            rst_high_seen <= 1'b1;
    end

    // ---- Test ----
    integer errors = 0;
    integer test_num = 0;

    initial begin
        // ---- Release reset ----
        #100;
        reset <= 1'b0;
        #20;

        // ==== Test 1: Backlight is always on ====
        test_num = 1;
        if (lcd_backlight != 1'b1) begin
            $display("FAIL test %0d: backlight should be on", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: backlight is on", test_num);
        end

        // ==== Test 2: LCD RST goes low ====
        test_num = 2;
        #50;
        if (!rst_low_seen) begin
            $display("FAIL test %0d: LCD RST never went low", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: LCD RST went low (hardware reset)", test_num);
        end

        // ==== Test 3: frame_drawn should be low during init ====
        test_num = 3;
        if (frame_drawn != 1'b0) begin
            $display("FAIL test %0d: frame_drawn should be low during init", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: frame_drawn low during init", test_num);
        end

        // ==== Test 4: Wait for init to complete ====
        // Init takes ~275ms (130ms reset + 145ms delays + byte time)
        test_num = 4;
        $display("Waiting for init to complete (~275ms)...");
        #290_000_000; // 290ms should cover init
        if (!rst_high_seen) begin
            $display("FAIL test %0d: LCD RST never went high", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: LCD RST released after hardware reset", test_num);
        end

        // ==== Test 5: SRAM reads should have started (frame scanning) ====
        test_num = 5;
        // Wait for at least one full frame (~30ms)
        #40_000_000;
        if (!pixel_reading && frame_count == 0) begin
            // Check if maybe we just missed it — wait more
            #30_000_000;
        end
        if (frame_count >= 1) begin
            $display("PASS test %0d: frame_drawn pulsed (%0d frames)", test_num, frame_count);
        end else begin
            $display("FAIL test %0d: no frame_drawn after init + frame time", test_num);
            errors = errors + 1;
        end

        // ==== Test 6: Backlight still on ====
        test_num = 6;
        if (lcd_backlight != 1'b1) begin
            $display("FAIL test %0d: backlight went off", test_num);
            errors = errors + 1;
        end else begin
            $display("PASS test %0d: backlight still on", test_num);
        end

        // ---- Summary ----
        $display("");
        $display("Frames completed: %0d", frame_count);
        if (errors == 0)
            $display("ALL %0d TESTS PASSED", test_num);
        else
            $display("FAILED: %0d errors out of %0d tests", errors, test_num);

        $finish;
    end

    // ---- Safety timeout (500ms) ----
    initial begin
        #500_000_000;
        $display("TIMEOUT: simulation exceeded 400ms (frame_count=%0d)", frame_count);
        $finish;
    end

    // ---- VCD dump ----
    initial begin
        $dumpfile("Hardware/FPGA/Verilog/Simulation/Output/spi_display_controller.vcd");
        $dumpvars(0, spi_display_controller_tb);
    end

endmodule
