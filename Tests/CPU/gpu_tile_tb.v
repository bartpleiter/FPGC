/*
 * gpu_tile_tb.v — Testbench for FrameScanEngine window tile compositing
 *
 * Instantiates FrameScanEngine with VRAM8, VRAM32, PixelPalette, and a
 * mock SPI master. Pre-loads VRAM with known test data (matching what
 * display_test.c writes) and traces the pixel pipeline output.
 *
 * Run: iverilog -o Tests/tmp/gpu_tile.out Tests/CPU/gpu_tile_tb.v && \
 *      vvp Tests/tmp/gpu_tile.out
 *
 * Debug: add $dumpfile/$dumpvars, then open VCD in GTKWave.
 */
`timescale 1ns / 1ps

module gpu_tile_tb;

    // ---- Clock: 100 MHz (10 ns period) ----
    reg clk = 0;
    always #5 clk = ~clk;

    reg reset = 1;
    reg enable = 0;

    // ---- VRAM8: 8194 × 8-bit ----
    reg  [13:0] vram8_cpu_addr = 0;
    reg  [7:0]  vram8_cpu_d = 0;
    reg         vram8_cpu_we = 0;
    wire [7:0]  vram8_cpu_q;

    wire [13:0] vram8_gpu_addr;
    wire [7:0]  vram8_gpu_q;

    // Simple dual-port BRAM model for VRAM8
    reg [7:0] vram8_mem [0:8193];
    integer vi;
    initial for (vi = 0; vi < 8194; vi = vi + 1) vram8_mem[vi] = 8'd0;

    always @(posedge clk) begin
        vram8_cpu_q_r <= vram8_mem[vram8_cpu_addr];
        if (vram8_cpu_we)
            vram8_mem[vram8_cpu_addr] <= vram8_cpu_d;
    end
    reg [7:0] vram8_cpu_q_r;
    assign vram8_cpu_q = vram8_cpu_q_r;

    reg [7:0] vram8_gpu_q_r;
    always @(posedge clk)
        vram8_gpu_q_r <= vram8_mem[vram8_gpu_addr];
    assign vram8_gpu_q = vram8_gpu_q_r;

    // ---- VRAM32: 1056 × 32-bit ----
    wire [10:0] vram32_gpu_addr;
    wire [31:0] vram32_gpu_q;

    reg [31:0] vram32_mem [0:1055];
    integer vj;
    initial for (vj = 0; vj < 1056; vj = vj + 1) vram32_mem[vj] = 32'd0;

    reg [31:0] vram32_gpu_q_r;
    always @(posedge clk)
        vram32_gpu_q_r <= vram32_mem[vram32_gpu_addr];
    assign vram32_gpu_q = vram32_gpu_q_r;

    // ---- VRAMPX mock: 76800 × 8-bit ----
    // Simplified: just return pixel_x[7:0] as indexed color
    wire [16:0] sram_addr;
    wire [7:0]  sram_data;
    wire        sram_read;

    reg [7:0] vrampx_mem [0:76799];
    integer vk;
    initial begin
        // Fill with position-unique values for alignment testing
        // Each pixel gets a unique palette index based on position
        for (vk = 0; vk < 76800; vk = vk + 1) begin
            vrampx_mem[vk] = vk[7:0];  // Low byte of linear address
        end
        // Override first few positions with distinctive values
        vrampx_mem[0]   = 8'h10;  // Position (0,0) — expect at pixel_count=0
        vrampx_mem[1]   = 8'h20;  // Position (1,0) — expect at pixel_count=1
        vrampx_mem[2]   = 8'h30;  // Position (2,0) — expect at pixel_count=2
        vrampx_mem[3]   = 8'h40;  // Position (3,0) — expect at pixel_count=3
        vrampx_mem[4]   = 8'h50;  // Position (4,0) — expect at pixel_count=4
        vrampx_mem[5]   = 8'h60;  // Position (5,0) — expect at pixel_count=5
        vrampx_mem[319] = 8'hA0;  // Position (319,0) — last column row 0
        vrampx_mem[320] = 8'hB0;  // Position (0,1) — first column row 1
        vrampx_mem[321] = 8'hC0;  // Position (1,1)
        vrampx_mem[322] = 8'hD0;  // Position (2,1)
    end

    reg [7:0] sram_data_r;
    always @(posedge clk)
        sram_data_r <= vrampx_mem[sram_addr];
    assign sram_data = sram_data_r;

    // ---- PixelPalette: 256 × 24-bit ----
    wire [7:0]  palette_idx;
    wire [23:0] palette_rgb;

    reg [23:0] palette_mem [0:255];
    integer pi;
    initial begin
        // Default RRRGGGBB → RGB24 mapping
        for (pi = 0; pi < 256; pi = pi + 1) begin
            palette_mem[pi] = {
                pi[7:5], pi[7:5], pi[7:6],
                pi[4:2], pi[4:2], pi[4:3],
                pi[1:0], pi[1:0], pi[1:0], pi[1:0]
            };
        end
    end

    reg [23:0] palette_rgb_r;
    always @(posedge clk)
        palette_rgb_r <= palette_mem[palette_idx];
    assign palette_rgb = palette_rgb_r;

    // ---- SPI mock: tx_ready toggles to simulate byte transmission ----
    reg spi_tx_ready = 1;
    wire [7:0] spi_tx_data;
    wire       spi_tx_valid;
    wire       spi_dc;
    wire       spi_cs_n;
    wire       frame_done;

    // Mock SPI: after tx_valid asserted, drop ready for 2 cycles then raise
    reg [2:0] spi_delay = 0;
    always @(posedge clk) begin
        if (spi_tx_valid && spi_tx_ready) begin
            spi_tx_ready <= 0;
            spi_delay <= 3'd2;
        end else if (spi_delay > 0) begin
            spi_delay <= spi_delay - 3'd1;
            if (spi_delay == 3'd1)
                spi_tx_ready <= 1;
        end
    end

    // ---- Device Under Test ----
    FrameScanEngine dut (
        .clk            (clk),
        .reset          (reset),
        .enable         (enable),
        .sram_addr      (sram_addr),
        .sram_data      (sram_data),
        .sram_data_valid(1'b1),
        .sram_read      (sram_read),
        .vram8_addr     (vram8_gpu_addr),
        .vram8_q        (vram8_gpu_q),
        .vram32_addr    (vram32_gpu_addr),
        .vram32_q       (vram32_gpu_q),
        .palette_idx    (palette_idx),
        .palette_rgb    (palette_rgb),
        .spi_tx_data    (spi_tx_data),
        .spi_tx_valid   (spi_tx_valid),
        .spi_tx_ready   (spi_tx_ready),
        .spi_dc         (spi_dc),
        .spi_cs_n       (spi_cs_n),
        .frame_done     (frame_done)
    );

    // ---- Pixel output capture ----
    // Track completed pixels by watching SPI byte pairs
    reg [7:0] captured_hi = 0;
    reg       got_hi = 0;
    integer pixel_count = 0;
    integer error_count = 0;

    // Capture palette_idx at composite time for alignment checking
    reg [7:0] composite_palette_idx = 0;
    always @(posedge clk) begin
        if (dut.state == 3'd2 && dut.pal_wait == 3'd6)
            composite_palette_idx <= palette_idx;
    end

    always @(posedge clk) begin
        if (spi_tx_valid && spi_tx_ready && spi_dc) begin
            if (!got_hi) begin
                captured_hi <= spi_tx_data;
                got_hi <= 1;
            end else begin
                // Complete pixel: captured_hi + spi_tx_data = RGB565
                got_hi <= 0;
                pixel_count <= pixel_count + 1;

                // Log first 6 pixels showing palette index for alignment check
                if (pixel_count < 6 || (pixel_count >= 319 && pixel_count < 325)) begin
                    $display("PIXEL %0d (x=%0d, y=%0d): RGB565=%04x  pal_idx=%0d  pixel_x_at_composite=%0d",
                        pixel_count,
                        pixel_count % 320,
                        pixel_count / 320,
                        {captured_hi, spi_tx_data},
                        composite_palette_idx,
                        dut.pixel_x);
                end

                // Also log HELLO region
                if (pixel_count >= 38534 && pixel_count < 38548) begin
                    $display("PIXEL %0d (x=%0d, y=%0d): RGB565=%04x  trans=%b",
                        pixel_count,
                        pixel_count % 320,
                        pixel_count / 320,
                        {captured_hi, spi_tx_data},
                        dut.win_transparent);
                end
            end
        end
    end

    // ---- Pipeline tracing (minimal) ----
    always @(posedge clk) begin
        if (dut.state == 3'd2 && dut.pal_wait == 3'd6) begin
            if (pixel_count >= 38534 && pixel_count < 38548) begin
                $display("  COMPOSITE px=%0d x=%0d y=%0d: pat_half=%04x col=%0d pat_bits=%b trans=%b palw=%08x",
                    pixel_count, dut.pixel_x, dut.pixel_y,
                    dut.win_pattern_half,
                    dut.col_in_tile,
                    dut.pattern_bits,
                    (dut.pattern_bits == 2'b00) && (dut.win_palette_word[31:24] == 8'd0),
                    dut.win_palette_word);
            end
        end
    end

    // ---- Load test data (matches display_test.c) ----
    task load_test_data;
        integer idx;
    begin
        $display("Loading test data into VRAMs...");

        // All VRAM8 and VRAM32 already zeroed by initial block

        // Write 'H' pattern (ASCII 72) at VRAM32 address 72*4 = 288
        vram32_mem[288] = 32'hF0F0F0F0;  // rows 0-1
        vram32_mem[289] = 32'hF0F0FFF0;  // rows 2-3
        vram32_mem[290] = 32'hF0F0F0F0;  // rows 4-5
        vram32_mem[291] = 32'hF0F00000;  // rows 6-7

        // Write 'E' pattern (ASCII 69) at VRAM32 address 69*4 = 276
        vram32_mem[276] = 32'hFFFC3C0C;
        vram32_mem[277] = 32'h3CC03FC0;
        vram32_mem[278] = 32'h3CC03C0C;
        vram32_mem[279] = 32'hFFFC0000;

        // Write 'L' pattern (ASCII 76) at VRAM32 address 76*4 = 304
        vram32_mem[304] = 32'hFF003C00;
        vram32_mem[305] = 32'h3C003C00;
        vram32_mem[306] = 32'h3C0C3C3C;
        vram32_mem[307] = 32'hFFFC0000;

        // Write 'O' pattern (ASCII 79) at VRAM32 address 79*4 = 316
        vram32_mem[316] = 32'h0FC03CF0;
        vram32_mem[317] = 32'hF03CF03C;
        vram32_mem[318] = 32'hF03C3CF0;
        vram32_mem[319] = 32'h0FC00000;

        // Write palette 0: white on transparent black (0x0000FFFF)
        // VRAM32 address 1024 = palette 0
        vram32_mem[1024] = 32'h0000FFFF;

        // Write HELLO tile indices at row 15, columns 17-21
        // VRAM8 address = 4096 + y*40 + x
        idx = 4096 + 15 * 40 + 17;
        vram8_mem[idx]     = 8'd72;  // H
        vram8_mem[idx + 1] = 8'd69;  // E
        vram8_mem[idx + 2] = 8'd76;  // L
        vram8_mem[idx + 3] = 8'd76;  // L
        vram8_mem[idx + 4] = 8'd79;  // O

        // Write color indices (palette 0 for all)
        idx = 6144 + 15 * 40 + 17;
        vram8_mem[idx]     = 8'd0;
        vram8_mem[idx + 1] = 8'd0;
        vram8_mem[idx + 2] = 8'd0;
        vram8_mem[idx + 3] = 8'd0;
        vram8_mem[idx + 4] = 8'd0;

        $display("Test data loaded.");
        $display("  Tile 'H' pattern[0] at VRAM32[288] = %08x", vram32_mem[288]);
        $display("  Palette 0 at VRAM32[1024] = %08x", vram32_mem[1024]);
        $display("  VRAM8[%0d] (HELLO tile 0) = %0d", 4096 + 15*40+17, vram8_mem[4096 + 15*40+17]);

        // Put a distinctive value at tile (0,0) to verify BRAM read latency
        vram8_mem[4096] = 8'hAA;  // Tile index 170 at position (0,0)
        $display("  VRAM8[4096] (latency test) = %02x", vram8_mem[4096]);
    end
    endtask

    // ---- Verify specific pixels ----
    // After first row of pixels, check that:
    // 1. Non-HELLO tiles are transparent (pixel palette color shows through)
    // 2. HELLO tiles show correct pattern

    // ---- Main simulation ----
    initial begin
        $dumpfile("Tests/tmp/gpu_tile.vcd");
        $dumpvars(0, gpu_tile_tb);

        $display("=== GPU Tile Compositing Testbench ===");

        // Load test data
        load_test_data;

        // Reset
        #100;
        reset = 0;
        #20;

        // Enable scanning
        enable = 1;
        $display("Scanning enabled at t=%0t", $time);

        // Run for enough cycles to render first few tile rows
        // Each pixel takes ~10 cycles (pipeline + SPI). 320 pixels per row.
        // We want to see at least 2 rows = 640 pixels = ~6400 cycles
        // Plus HELLO at row 15 = 15*320 = 4800 pixels before HELLO starts
        // Need ~60000 cycles minimum for row 15

        // Run for 10M ns to reach HELLO at pixel ~38536 (each pixel ~160ns)
        #10000000;

        $display("\n=== Simulation complete ===");
        $display("Total pixels rendered: %0d", pixel_count);
        $display("Errors detected: %0d", error_count);

        $finish;
    end

    // Timeout safety
    initial begin
        #50000000;
        $display("TIMEOUT: Simulation exceeded 50M ns");
        $finish;
    end

endmodule
