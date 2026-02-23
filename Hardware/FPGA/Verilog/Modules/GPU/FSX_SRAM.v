/*
 * FSX_SRAM
 * Frame Synthesizer using external SRAM for pixel framebuffer
 */
module FSX_SRAM (
    // Clocks
    input wire          clk_pixel,    // Pixel clock (25MHz)
    input wire          clk_tmds_half, // Half of HDMI TDMS clock (pre-ddr)

    // HDMI
    output wire         tmds_clk_p,
    output wire         tmds_clk_n,
    output wire         tmds_d0_p,
    output wire         tmds_d0_n,
    output wire         tmds_d1_p,
    output wire         tmds_d1_n,
    output wire         tmds_d2_p,
    output wire         tmds_d2_n,

    // VRAM32 (for BGW renderer)
    output wire [10:0]  vram32_addr,
    input wire  [31:0]  vram32_q,

    // VRAM8 (for BGW renderer)
    output wire [13:0]  vram8_addr,
    input wire  [7:0]   vram8_q,

    // Pixel SRAM interface
    output wire [16:0]  pixel_sram_addr,   // Address request to SRAM
    input wire  [7:0]   pixel_sram_data,   // Data from SRAM
    output wire         pixel_using_line_buffer, // High when GPU uses line buffer
    
    // Timing outputs for SRAM arbiter
    output wire [11:0]  h_count_out,
    output wire [11:0]  v_count_out,
    output wire         vsync_out,
    output wire         blank_out,

    // Parameters
    input wire          half_res, // Render half res at full res by zooming in at top left corner

    // Interrupt signal
    output wire         frame_drawn
);

// ---- Video timings ----
wire [11:0] h_count; // Line position in pixels including blanking 
wire [11:0] v_count; // Frame position in lines including blanking 
wire hsync;
wire vsync;
wire blank;

assign h_count_out = h_count;
assign v_count_out = v_count;
assign vsync_out = vsync;
assign blank_out = blank;

TimingGenerator timing_generator (
    .clk_pixel  (clk_pixel),
    .h_count    (h_count),
    .v_count    (v_count),
    .hsync      (hsync),
    .vsync      (vsync),
    .blank      (blank),
    .frame_drawn(frame_drawn)
);

// ---- BGW plane ----
wire [2:0] bgw_r;
wire [2:0] bgw_g;
wire [1:0] bgw_b;

BGWrenderer bgw_renderer (
    .clk_pixel  (clk_pixel),
    .vs         (vsync),
    .h_count    (h_count),
    .v_count    (v_count),
    .r          (bgw_r),
    .g          (bgw_g),
    .b          (bgw_b),
    .vram32_addr(vram32_addr),
    .vram32_q   (vram32_q),
    .vram8_addr (vram8_addr),
    .vram8_q    (vram8_q)
);

// ---- Pixel plane ----
wire [2:0] px_r;
wire [2:0] px_g;
wire [1:0] px_b;

PixelEngineSRAM pixel_engine (
    .clk_pixel         (clk_pixel),
    .blank             (blank),
    .h_count           (h_count),
    .v_count           (v_count),
    .r                 (px_r),
    .g                 (px_g),
    .b                 (px_b),
    .sram_addr         (pixel_sram_addr),
    .sram_data         (pixel_sram_data),
    .using_line_buffer (pixel_using_line_buffer),
    .half_res          (half_res)
);

// ---- Priority and combining ----
wire px_priority = (bgw_r == 3'd0 && bgw_g == 3'd0 && bgw_b == 2'd0);

wire [2:0] r_combined;
wire [2:0] g_combined;
wire [1:0] b_combined;

assign r_combined = (px_priority) ? px_r : bgw_r;
assign g_combined = (px_priority) ? px_g : bgw_g;
assign b_combined = (px_priority) ? px_b : bgw_b;

// ---- RGB conversion ----
wire [7:0] r_byte;
wire [7:0] g_byte;
wire [7:0] b_byte;

RGB8toRGB24 rgb8_to_24 (
    .rgb8  ({r_combined, g_combined, b_combined}),
    .rgb24 ({r_byte, g_byte, b_byte})
);


// ---- HDMI output ----
`ifndef __ICARUS__
RGB2HDMI rgb2hdmi (
    .clk_tmds_half (clk_tmds_half),
    .clk_rgb       (clk_pixel),
    .r_rgb         (r_byte),
    .g_rgb         (g_byte),
    .b_rgb         (b_byte),
    .blk           (blank),
    .hs            (hsync),
    .vs            (vsync),
    .tmds_clk_p    (tmds_clk_p),
    .tmds_clk_n    (tmds_clk_n),
    .tmds_d0_p     (tmds_d0_p),
    .tmds_d0_n     (tmds_d0_n),
    .tmds_d1_p     (tmds_d1_p),
    .tmds_d1_n     (tmds_d1_n),
    .tmds_d2_p     (tmds_d2_p),
    .tmds_d2_n     (tmds_d2_n)
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
            "/home/bart/repos/FPGC/Hardware/FPGA/Verilog/Simulation/Output/frame%0d.ppm",
            framecounter
        ),
        "w"
    );
    $fwrite(file, "P3\n");
    $fwrite(file, "640 480\n");
    $fwrite(file, "255\n");
    framecounter = framecounter + 1;
end

always @(posedge clk_pixel)
begin
    if (~blank)
    begin
        $fwrite(file, "%d  %d  %d\n", r_byte, g_byte, b_byte);
    end
end

`endif

endmodule
