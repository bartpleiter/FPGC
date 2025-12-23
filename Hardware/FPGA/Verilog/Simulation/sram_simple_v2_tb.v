/*
 * Simple Testbench for SRAM V2 Pixel Framebuffer
 * 
 * Tests the simplified external SRAM pixel framebuffer design without CPU.
 * Pre-initializes SRAM with a test pattern and verifies GPU output.
 * 
 * Test pattern: Incrementing R3G3B2 values (0, 1, 2, ... 255, 0, 1, ...)
 * This creates a gradient pattern across the screen.
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/Memory/IS61LV5128AL.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SRAMWriteFIFOBlock.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SRAMArbiterV2.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/VRAMPXSramV2.v"

`include "Hardware/FPGA/Verilog/Modules/Memory/VRAM.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/TimingGenerator.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/BGWrenderer.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/PixelEngineSRAMV2.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/FSX_SRAM_V2.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/HDMI/RGB8toRGB24.v"

module sram_simple_v2_tb ();

//=============================================================================
// Clock Generation
// All clocks derived from same "PLL" - phase aligned
//=============================================================================
reg clk100 = 1'b0;       // 100MHz arbiter clock
reg clk50 = 1'b0;        // 50MHz CPU clock
reg clkPixel = 1'b0;     // 25MHz GPU clock
reg reset = 1'b1;

// 100 MHz clock (10ns period)
always #5 clk100 = ~clk100;

// 50 MHz clock (20ns period) - aligned with clk100
always #10 clk50 = ~clk50;

// 25 MHz clock (40ns period) - aligned with clk100/clk50
always #20 clkPixel = ~clkPixel;

//=============================================================================
// External SRAM
//=============================================================================
wire [18:0] SRAM_A;
wire [7:0]  SRAM_DQ;
wire        SRAM_CSn;
wire        SRAM_OEn;
wire        SRAM_WEn;

IS61LV5128AL #(
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/sram_test.list")
) sram (
    .A(SRAM_A),
    .DQ(SRAM_DQ),
    .CE_n(SRAM_CSn),
    .OE_n(SRAM_OEn),
    .WE_n(SRAM_WEn)
);

// No external initialization needed - SRAM loads from file

//=============================================================================
// VRAMPX SRAM Interface V2
//=============================================================================
reg [16:0] cpu_addr_reg = 17'd0;
reg [7:0]  cpu_data_reg = 8'd0;
reg        cpu_we_reg = 1'b0;

// GPU interface
wire [16:0] gpu_pixel_addr;
wire [7:0]  gpu_pixel_data;

// Timing signals
wire [11:0] fsx_h_count;
wire [11:0] fsx_v_count;
wire        fsx_vsync;
wire        fsx_blank;

VRAMPXSramV2 vrampx_sram (
    // Clocks and reset
    .clk50(clk50),
    .clk100(clk100),
    .clkPixel(clkPixel),
    .reset(reset),
    
    // CPU interface
    .cpu_addr(cpu_addr_reg),
    .cpu_data(cpu_data_reg),
    .cpu_we(cpu_we_reg),
    
    // GPU interface
    .gpu_addr(gpu_pixel_addr),
    .gpu_data(gpu_pixel_data),
    
    // GPU timing
    .blank(fsx_blank),
    .vsync(fsx_vsync),
    
    // External SRAM
    .SRAM_A(SRAM_A),
    .SRAM_DQ(SRAM_DQ),
    .SRAM_CSn(SRAM_CSn),
    .SRAM_OEn(SRAM_OEn),
    .SRAM_WEn(SRAM_WEn)
);

//=============================================================================
// VRAM32 (for BGW renderer - initialized to black)
//=============================================================================
wire [10:0] vram32_gpu_addr;
wire [31:0] vram32_gpu_q = 32'd0;  // Return black for BGW

//=============================================================================
// VRAM8 (for BGW renderer - initialized to black)
//=============================================================================
wire [13:0] vram8_gpu_addr;
wire [7:0]  vram8_gpu_q = 8'd0;  // Return black for BGW

//=============================================================================
// FSX_SRAM_V2 (GPU)
//=============================================================================
wire frameDrawn;

FSX_SRAM_V2 fsx (
    .clkPixel(clkPixel),
    .clkTMDShalf(clk100),

    // HDMI outputs (unused in simulation)
    .TMDS_clk_p(),
    .TMDS_clk_n(),
    .TMDS_d0_p(),
    .TMDS_d0_n(),
    .TMDS_d1_p(),
    .TMDS_d1_n(),
    .TMDS_d2_p(),
    .TMDS_d2_n(),

    // VRAM32
    .vram32_addr(vram32_gpu_addr),
    .vram32_q   (vram32_gpu_q),

    // VRAM8
    .vram8_addr(vram8_gpu_addr),
    .vram8_q   (vram8_gpu_q),

    // Pixel SRAM interface
    .pixel_sram_addr(gpu_pixel_addr),
    .pixel_sram_data(gpu_pixel_data),
    
    // Timing outputs
    .h_count_out(fsx_h_count),
    .v_count_out(fsx_v_count),
    .vsync_out(fsx_vsync),
    .blank_out(fsx_blank),

    // Parameters
    .halfRes(1'b0),

    // Interrupt
    .frameDrawn(frameDrawn)
);

//=============================================================================
// Simulation Control
//=============================================================================
integer clk_counter = 0;
integer frame_count = 0;

always @(posedge clk100) begin
    clk_counter = clk_counter + 1;
end

// Count frames
always @(negedge fsx_vsync) begin
    frame_count = frame_count + 1;
    $display("Frame %0d completed at time %0t", frame_count, $time);
end

// Release reset after 100 cycles
initial begin
    #1000 reset = 1'b0;
    $display("Reset released at time %0t", $time);
    // Debug: check SRAM contents after loading
    $display("SRAM[0]=%02h SRAM[1]=%02h SRAM[2]=%02h SRAM[3]=%02h SRAM[4]=%02h", 
             sram.mem[0], sram.mem[1], sram.mem[2], sram.mem[3], sram.mem[4]);
end

// CPU write test - write a different value to pixel 0 during frame 2
// Should show up in frame 3
always @(posedge clk50) begin
    if (!reset && frame_count == 2 && fsx_blank) begin
        // Write 0xFF (white) to pixel 0 during blanking
        cpu_addr_reg <= 17'd0;
        cpu_data_reg <= 8'hFF;
        cpu_we_reg <= 1'b1;
    end else begin
        cpu_we_reg <= 1'b0;
    end
end

// Run for enough time to see at least 3 real frames
// VGA 640x480 @ 25MHz = 800*525 = 420000 GPU clocks per frame
// At 25MHz (40ns period), that's ~16.8ms per frame
// At 100MHz (10ns), that's ~420000 * 4 = 1.68M cycles per frame
initial begin
    $dumpfile("Hardware/FPGA/Verilog/Simulation/Output/sram_simple_v2.vcd");
    $dumpvars(0, sram_simple_v2_tb);
    
    // Run for about 55ms (3+ frames)
    #55000000;
    $display("Simulation finished after %0d clk100 cycles", clk_counter);
    $finish;
end

// Debug output - sample pixels around the blanking-to-active transition
reg [3:0] debug_sample_count = 0;
always @(posedge clkPixel) begin
    // Show transition from blanking to active (h=156-170) for frames 2 and 3
    // Also show first two lines to verify vertical scaling
    if (((fsx_v_count == 45 || fsx_v_count == 46) && fsx_h_count >= 160 && fsx_h_count <= 170) && 
        (frame_count >= 2 && frame_count <= 3)) begin
        $display("t=%0t FRAME=%0d h=%0d v=%0d addr=%0d data=%02h blank=%b sram_addr=%h sram_dq=%02h", 
                 $time, frame_count, fsx_h_count, fsx_v_count, gpu_pixel_addr, gpu_pixel_data, fsx_blank,
                 SRAM_A[16:0], SRAM_DQ);
    end
end

endmodule
