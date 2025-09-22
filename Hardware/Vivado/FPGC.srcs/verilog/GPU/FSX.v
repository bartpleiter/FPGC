/*
 * Graphical processor (Frame Synthesizer)
 * Generates video from VRAM
 */
module FSX (
    // Clocks and reset
    input wire clkPixel,    // Pixel clock
    input wire clkTMDShalf, // Half of HDMI TDMS clock (pre-ddr)
    input wire reset,

    // HDMI
    output wire TMDS_clk_p,
    output wire TMDS_clk_n,
    output wire TMDS_d0_p,
    output wire TMDS_d0_n,
    output wire TMDS_d1_p,
    output wire TMDS_d1_n,
    output wire TMDS_d2_p,
    output wire TMDS_d2_n,

    // VRAM32
    output wire [10:0] vram32_addr,
    input wire [31:0]  vram32_q,

    // VRAM8
    output wire [13:0] vram8_addr,
    input wire [7:0]   vram8_q,

    // VRAMpixel
    output wire [16:0] vramPX_addr,
    input wire [7:0]   vramPX_q,

    // Parameters
    input wire halfRes, // Render half res at full res by zooming in at top left corner
    
    // Interrupt signal
    output wire frameDrawn
);

// Video timings
wire [11:0] h_count; // Line position in pixels including blanking 
wire [11:0] v_count; // Frame position in lines including blanking 
wire hsync;
wire vsync;
wire blank;

TimingGenerator timingGenerator (
    // Clock
    .clkPixel(clkPixel),

    // Position counters
    .h_count(h_count),
    .v_count(v_count),

    // Video signals
    .hsync(hsync),
    .vsync(vsync),
    .csync(csync),
    .blank(blank),

    // Interrupt signal
    .frameDrawn(frameDrawn)
);

// RGB values for Background and Window (BGW) plane
wire [2:0] BGW_r;
wire [2:0] BGW_g;
wire [1:0] BGW_b;

BGWrenderer bgwrenderer (
    // Clock
    .clkPixel(clkPixel),

    // Video timings
    .vs(vsync),
    .h_count(h_count),
    .v_count(v_count),

    // Output colors
    .r(BGW_r),
    .g(BGW_g),
    .b(BGW_b),

    // VRAM32
    .vram32_addr(vram32_addr),
    .vram32_q(vram32_q),

    // VRAM8
    .vram8_addr(vram8_addr),
    .vram8_q(vram8_q)
);

// RGB values for Pixel (PX) plane 
wire [2:0] PX_r;
wire [2:0] PX_g;
wire [1:0] PX_b;

PixelEngine pixelEngine (
    // Video I/O
    .blank  (blank),
    .halfRes(halfRes),
    .h_count(h_count),
    .v_count(v_count),

    // Output colors
    .r(PX_r),
    .g(PX_g),
    .b(PX_b),

    // VRAMpixel
    .vram_addr(vramPX_addr),
    .vram_q(vramPX_q)
);

// Give priority to pixel plane if bgw plane is black
wire pxPriority = (BGW_r == 3'd0 && BGW_g == 3'd0 && BGW_b == 2'd0);

// Combine RGB values from BGW and PX planes based on priority
wire [2:0] r_combined;
wire [2:0] g_combined;
wire [1:0] b_combined;

assign r_combined = (pxPriority) ? PX_r : BGW_r;
assign g_combined = (pxPriority) ? PX_g : BGW_g;
assign b_combined = (pxPriority) ? PX_b : BGW_b;

// Convert R3G3B2 values to 8-bit
wire [7:0] r_byte;
wire [7:0] g_byte;
wire [7:0] b_byte;

RGB8toRGB24 rgb8to24 (
    .rgb8 ({r_combined, g_combined, b_combined}),
    .rgb24({r_byte, g_byte, b_byte})
);


// Ignore TMDS serialization if using Icarus Verilog as it uses Xilinx primitives
`ifndef __ICARUS__
// Convert VGA signal to HDMI signals
RGB2HDMI rgb2hdmi (
    .clkTMDShalf(clkTMDShalf),
    .clkRGB     (clkPixel),
    .reset      (reset),
    .rRGB       (r_byte),
    .gRGB       (g_byte),
    .bRGB       (b_byte),
    .blk        (blank),
    .hs         (hsync),
    .vs         (vsync),
    .TMDS_clk_p (TMDS_clk_p),
    .TMDS_clk_n (TMDS_clk_n),
    .TMDS_d0_p  (TMDS_d0_p),
    .TMDS_d0_n  (TMDS_d0_n),
    .TMDS_d1_p  (TMDS_d1_p),
    .TMDS_d1_n  (TMDS_d1_n),
    .TMDS_d2_p  (TMDS_d2_p),
    .TMDS_d2_n  (TMDS_d2_n)
);
`endif

// Image file generator for simulation
`ifdef __ICARUS__

integer file;
integer framecounter = 0;

always @(negedge vsync)
begin
    file = $fopen(
        $sformatf(
            "/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/output/frame%0d.ppm",
            framecounter
        ),
        "w"
    );
    $fwrite(file, "P3\n");
    $fwrite(file, "640 480\n");
    $fwrite(file, "255\n");
    framecounter = framecounter + 1;
end

always @(posedge clkPixel)
begin
    if (~blank)
    begin
        $fwrite(file, "%d  %d  %d\n", r_byte, g_byte, b_byte);
    end
end

`endif

endmodule
