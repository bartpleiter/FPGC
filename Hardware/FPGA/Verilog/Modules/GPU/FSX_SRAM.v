/*
 * FSX_SRAM
 * Frame Synthesizer using external SRAM for pixel framebuffer
 */
module FSX_SRAM (
    // Clocks
    input wire          clk_pixel,    // Pixel clock (25MHz)
    input wire          clk_tmds_half, // Half of HDMI TDMS clock (pre-ddr)
    input wire          clk_sys,      // System clock (100MHz) for palette writes

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

    // Palette CPU write port
    input wire          palette_we,
    input wire  [7:0]   palette_addr,
    input wire  [23:0]  palette_wdata,

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

// Guard band: tell arbiter to stop writes 2 pixel clocks before active video.
// This ensures sram_data_reg has valid read data before the palette captures
// the first pixel. Without this, the write-to-read transition can leave
// sram_data_reg with stale/undefined data from the SRAM bus turnaround.
// Uses same constants as TimingGenerator: HA_STA=159, HA_END=799, VA_STA=44, VA_END=524
wire arbiter_active = (h_count > 12'd157) && (h_count <= 12'd799) &&
                      (v_count > 12'd44)  && (v_count <= 12'd524);
assign blank_out = ~arbiter_active;

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

// ---- RGB conversion via programmable palette ----
wire [7:0] r_byte;
wire [7:0] g_byte;
wire [7:0] b_byte;

PixelPalette px_palette (
    // GPU read port (25 MHz)
    .clk_pixel (clk_pixel),
    .gpu_index ({r_combined, g_combined, b_combined}),
    .gpu_rgb24 ({r_byte, g_byte, b_byte}),

    // CPU write port (100 MHz)
    .clk_sys   (clk_sys),
    .cpu_we    (palette_we),
    .cpu_addr  (palette_addr),
    .cpu_wdata (palette_wdata)
);

// ---- Sync signal delay (1 cycle to match palette BRAM read latency) ----
reg        blank_d = 1'b1;
reg        hsync_d = 1'b0;
reg        vsync_d = 1'b0;

always @(posedge clk_pixel) begin
    blank_d <= blank;
    hsync_d <= hsync;
    vsync_d <= vsync;
end


// ---- HDMI output ----
`ifndef __ICARUS__
RGB2HDMI rgb2hdmi (
    .clk_tmds_half (clk_tmds_half),
    .clk_rgb       (clk_pixel),
    .r_rgb         (r_byte),
    .g_rgb         (g_byte),
    .b_rgb         (b_byte),
    .blk           (blank_d),
    .hs            (hsync_d),
    .vs            (vsync_d),
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

always @(negedge vsync_d)
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
    if (~blank_d)
    begin
        $fwrite(file, "%d  %d  %d\n", r_byte, g_byte, b_byte);
    end
end

`endif

endmodule
