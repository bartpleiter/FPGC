/*
 * Testbench for pixel pipeline glitch investigation.
 * Tests PixelEngineSRAM + VRAMPXSram + FSX_SRAM palette with proper
 * 4:1 clock ratio (25MHz pixel, 100MHz system) and concurrent CPU writes.
 * Generated testbench for an issue that has been resolved, but can still be
 * useful when creating a new GPU testbench with the external SRAM interface.
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/GPU/FSX_SRAM.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/PixelEngineSRAM.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/PixelPalette.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/TimingGenerator.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/BGWrenderer.v"
// RGB2HDMI and TMDSenc are skipped in simulation (ifdef __ICARUS__)
`include "Hardware/FPGA/Verilog/Modules/Memory/VRAMPXSram.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SRAMArbiter.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SyncFIFO.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/IS61LV5128AL.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/VRAM.v"

module pixel_pipeline_tb ();

    // ---- Clocks ----
    // 100MHz system clock (10ns period)
    reg clk100 = 1'b0;
    always #5 clk100 = ~clk100;
    
    // 25MHz pixel clock (40ns period), phase-aligned with clk100
    // Rises on every 4th clk100 rising edge
    reg clk_pixel = 1'b0;
    reg [1:0] clk_div = 2'd0;
    always @(posedge clk100) begin
        clk_div <= clk_div + 1;
        if (clk_div == 2'd1)
            clk_pixel <= 1'b1;
        else if (clk_div == 2'd3)
            clk_pixel <= 1'b0;
    end

    reg reset = 1'b0;

    // ---- External SRAM interface ----
    wire [18:0] SRAM_A;
    wire [7:0]  SRAM_DQ;
    wire        SRAM_CSn;
    wire        SRAM_OEn;
    wire        SRAM_WEn;

    // ---- SRAM model with realistic access time delay ----
    // Real IS61LV5128AL has 10ns access time
    // Add delay to model this
    wire [7:0] SRAM_DQ_immediate;
    wire [7:0] SRAM_DQ_delayed;
    
    IS61LV5128AL sram (
        .A     (SRAM_A),
        .DQ    (SRAM_DQ),
        .CE_n  (SRAM_CSn),
        .OE_n  (SRAM_OEn),
        .WE_n  (SRAM_WEn)
    );

    // ---- GPU signals ----
    wire [16:0] gpu_pixel_addr;
    wire [7:0]  gpu_pixel_data;
    wire        gpu_pixel_using_line_buffer;
    wire [11:0] h_count;
    wire [11:0] v_count;
    wire        vsync;
    wire        blank;
    wire        frame_drawn;

    // ---- CPU write interface ----
    reg  [16:0] cpu_addr = 17'd0;
    reg  [7:0]  cpu_data = 8'd0;
    reg         cpu_we   = 1'b0;
    wire        cpu_fifo_full;

    // ---- VRAMPXSram ----
    VRAMPXSram vrampx_sram (
        .clk100     (clk100),
        .clk_pixel  (clk_pixel),
        .reset      (reset),
        .cpu_addr   (cpu_addr),
        .cpu_data   (cpu_data),
        .cpu_we     (cpu_we),
        .gpu_addr   (gpu_pixel_addr),
        .gpu_data   (gpu_pixel_data),
        .using_line_buffer (gpu_pixel_using_line_buffer),
        .blank      (blank),
        .vsync      (vsync),
        .cpu_fifo_full (cpu_fifo_full),
        .SRAM_A     (SRAM_A),
        .SRAM_DQ    (SRAM_DQ),
        .SRAM_CSn   (SRAM_CSn),
        .SRAM_OEn   (SRAM_OEn),
        .SRAM_WEn   (SRAM_WEn)
    );

    // ---- VRAM32 for BGW (all zeros = transparent) ----
    wire [10:0] vram32_addr;
    wire [31:0] vram32_q;
    
    // Simple zero-filled VRAM32 - BGW will output all zeros (transparent)
    VRAM #(
        .WIDTH(32),
        .WORDS(1056),
        .ADDR_BITS(11),
        .LIST("")
    ) vram32 (
        .cpu_clk  (clk100),
        .cpu_d    (32'd0),
        .cpu_addr (11'd0),
        .cpu_we   (1'b0),
        .cpu_q    (),
        .gpu_clk  (clk_pixel),
        .gpu_d    (32'd0),
        .gpu_addr (vram32_addr),
        .gpu_we   (1'b0),
        .gpu_q    (vram32_q)
    );

    // ---- VRAM8 for BGW (all zeros) ----
    wire [13:0] vram8_addr;
    wire [7:0]  vram8_q;

    VRAM #(
        .WIDTH(8),
        .WORDS(8194),
        .ADDR_BITS(14),
        .LIST("")
    ) vram8 (
        .cpu_clk  (clk100),
        .cpu_d    (8'd0),
        .cpu_addr (14'd0),
        .cpu_we   (1'b0),
        .cpu_q    (),
        .gpu_clk  (clk_pixel),
        .gpu_d    (8'd0),
        .gpu_addr (vram8_addr),
        .gpu_we   (1'b0),
        .gpu_q    (vram8_q)
    );

    // ---- FSX_SRAM (Frame Synthesizer) ----
    FSX_SRAM fsx (
        .clk_pixel      (clk_pixel),
        .clk_tmds_half  (clk100),      // Not used in simulation
        .clk_sys        (clk100),
        
        // VRAM32 for BGW
        .vram32_addr    (vram32_addr),
        .vram32_q       (vram32_q),
        
        // VRAM8 for BGW
        .vram8_addr     (vram8_addr),
        .vram8_q        (vram8_q),
        
        // Pixel SRAM interface
        .pixel_sram_addr  (gpu_pixel_addr),
        .pixel_sram_data  (gpu_pixel_data),
        .pixel_using_line_buffer (gpu_pixel_using_line_buffer),
        
        // Timing outputs
        .h_count_out    (h_count),
        .v_count_out    (v_count),
        .vsync_out      (vsync),
        .blank_out       (blank),
        
        .half_res       (1'b0),
        
        // Palette write port (not used in this test)
        .palette_we     (1'b0),
        .palette_addr   (8'd0),
        .palette_wdata  (24'd0),
        
        .frame_drawn    (frame_drawn)
    );

    // ---- Monitoring ----
    // Track palette input and output for glitch detection
    wire [7:0] palette_index = {fsx.r_combined, fsx.g_combined, fsx.b_combined};
    wire [7:0] r_byte = fsx.r_byte;
    wire [7:0] g_byte = fsx.g_byte;
    wire [7:0] b_byte = fsx.b_byte;
    wire        blank_d = fsx.blank_d;
    
    // Track pixel engine internals
    wire [7:0] display_pixel_raw = {fsx.pixel_engine.r, fsx.pixel_engine.g, fsx.pixel_engine.b};
    wire [7:0] sram_data_from_arbiter = gpu_pixel_data;
    wire [7:0] sram_data_reg = vrampx_sram.arbiter.sram_data_reg;
    wire [1:0] write_state = vrampx_sram.arbiter.write_state;
    wire       can_write = vrampx_sram.arbiter.can_write;
    wire       fifo_empty = vrampx_sram.arbiter.cpu_fifo_empty;
    
    // Expected pixel value: each pixel address contains its own index mod 256
    // So pixel at address A has value (A & 0xFF)
    wire [16:0] expected_addr = fsx.pixel_engine.pixel_addr;
    wire [7:0]  expected_data = expected_addr[7:0];
    
    // Glitch detection: compare displayed pixel to expected value
    // Only check first pixel of each line (the problematic one)
    wire [9:0]  pixel_active = fsx.pixel_engine.pixel_active;
    wire        in_active_video = fsx.pixel_engine.in_active_video;
    wire        first_pixel_of_pair = fsx.pixel_engine.first_pixel_of_pair;
    wire        first_line_of_pair = fsx.pixel_engine.first_line_of_pair;

    // Log glitches during active video region
    integer glitch_count = 0;
    integer pixel_count = 0;
    integer frame_count = 0;
    
    // Check at the moment the palette output is used (1 cycle after palette input)
    // This is when blank_d is low (active video after delay)
    reg check_enable = 0;
    reg [7:0] prev_palette_index = 0;
    reg [16:0] prev_expected_addr = 0;
    
    always @(posedge clk_pixel) begin
        // Capture the palette index for checking next cycle
        if (!blank && in_active_video) begin
            check_enable <= 1;
            prev_palette_index <= palette_index;
            prev_expected_addr <= expected_addr;
        end else begin
            check_enable <= 0;
        end
        
        // Check if the palette output matches what we expect
        // The palette is identity-mapped (default initialization)
        // So palette output for index I should be RGB8toRGB24(I)
        if (check_enable && !blank_d) begin
            pixel_count <= pixel_count + 1;
            // Check if the palette INDEX was wrong (not the output conversion)
            // The actual pixel data should match what's in SRAM
            if (prev_palette_index !== prev_expected_addr[7:0]) begin
                glitch_count <= glitch_count + 1;
                $display("GLITCH @%t: h=%0d v=%0d pixel_active=%0d addr=%0h expected_idx=0x%02h got_idx=0x%02h sram_data_reg=0x%02h write_state=%0d can_write=%0d fifo_empty=%0d",
                    $time, h_count, v_count,
                    pixel_active,
                    prev_expected_addr,
                    prev_expected_addr[7:0],
                    prev_palette_index,
                    sram_data_reg,
                    write_state,
                    can_write,
                    fifo_empty);
            end
        end
    end

    // Frame counter
    always @(posedge frame_drawn) begin
        if (frame_count > 0) begin
            $display("Frame %0d complete: %0d glitches / %0d pixels", 
                     frame_count, glitch_count, pixel_count);
        end
        frame_count <= frame_count + 1;
        glitch_count <= 0;
        pixel_count <= 0;
    end

    // ---- Test sequence ----
    integer i;
    
    initial begin
        $dumpfile("Hardware/FPGA/Verilog/Simulation/Output/pixel_pipeline.vcd");
        $dumpvars(0, pixel_pipeline_tb);
        
        // Initialize SRAM with known pattern
        // Each address A gets value (A & 0xFF)
        for (i = 0; i < 76800; i = i + 1) begin
            sram.mem[i] = i[7:0];
        end
        
        // Wait for reset
        #100;
        
        // Wait for first frame to start (skip initial transients)
        // Wait until we're in active video
        @(posedge frame_drawn);
        $display("First frame started at %t", $time);
        
        // During second frame, start writing to SRAM continuously via CPU
        // This simulates the real-world case where a program writes pixels
        @(posedge frame_drawn);
        $display("Starting CPU writes at %t", $time);
        
        // Enable continuous writes
        cpu_write_enable = 1;
        
        // Run for 3 more frames then stop
        @(posedge frame_drawn);
        @(posedge frame_drawn);
        @(posedge frame_drawn);
        
        $display("Simulation complete. Total frames: %0d", frame_count);
        $finish;
    end

    // Continuous CPU write process
    reg cpu_write_enable = 0;
    reg [31:0] wr_addr_counter = 0;
    
    always @(posedge clk100) begin
        if (cpu_write_enable && !cpu_fifo_full) begin
            cpu_we <= 1'b1;
            cpu_addr <= wr_addr_counter[16:0];
            cpu_data <= wr_addr_counter[7:0];
            if (wr_addr_counter >= 76799)
                wr_addr_counter <= 0;
            else
                wr_addr_counter <= wr_addr_counter + 1;
        end else begin
            cpu_we <= 1'b0;
        end
    end

endmodule
