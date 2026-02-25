/*
 * RGB2HDMI
 * VGA RGBHS to HDMI signal converter
 */
module RGB2HDMI (
    // Clocks
    input wire          clk_tmds_half,
    input wire          clk_rgb,

    // RGB
    input wire  [7:0]   r_rgb,
    input wire  [7:0]   g_rgb,
    input wire  [7:0]   b_rgb,
    input wire          blk,
    input wire          hs,
    input wire          vs,

    // HDMI
    output wire         tmds_clk_p,
    output wire         tmds_clk_n,
    output wire         tmds_d0_p,
    output wire         tmds_d0_n,
    output wire         tmds_d1_p,
    output wire         tmds_d1_n,
    output wire         tmds_d2_p,
    output wire         tmds_d2_n
);

// ---- TMDS encoding ----
wire [9:0] encoded_red;
wire [9:0] encoded_green;
wire [9:0] encoded_blue;

TMDSenc tmds_r (
    .clk  (clk_rgb),
    .data (r_rgb),
    .c    (2'd0),
    .blk  (blk),
    .q    (encoded_red)
);

TMDSenc tmds_g (
    .clk  (clk_rgb),
    .data (g_rgb),
    .c    (2'd0),
    .blk  (blk),
    .q    (encoded_green)
);

TMDSenc tmds_b (
    .clk  (clk_rgb),
    .data (b_rgb),
    .c    ({vs, hs}),
    .blk  (blk),
    .q    (encoded_blue)
);

// ---- DDR serializer ----
reg [9:0] latched_red    = 10'd0;
reg [9:0] latched_green  = 10'd0;
reg [9:0] latched_blue   = 10'd0;

reg [9:0] shift_red      = 10'd0;
reg [9:0] shift_green    = 10'd0;
reg [9:0] shift_blue     = 10'd0;

reg [9:0] shift_clk      = 10'b0000011111;

always @(posedge clk_rgb)
begin
    latched_red   <= encoded_red;
    latched_green <= encoded_green;
    latched_blue  <= encoded_blue;
end

always @(posedge clk_tmds_half)
begin
    if (shift_clk == 10'b0000011111)
    begin
        shift_red   <= latched_red;
        shift_green <= latched_green;
        shift_blue  <= latched_blue;
    end
    else
    begin
        shift_red   <= {2'b00, shift_red[9:2]};
        shift_green <= {2'b00, shift_green[9:2]};
        shift_blue  <= {2'b00, shift_blue[9:2]};
    end
        shift_clk <= {shift_clk[1:0], shift_clk[9:2]};
end

// ---- DDR output ----
ddr ddr_r (
    .outclock (clk_tmds_half),
    .datain_h (shift_red[0]),
    .datain_l (shift_red[1]),
    .dataout  (tmds_d2_p)
);

ddr ddr_g (
    .outclock (clk_tmds_half),
    .datain_h (shift_green[0]),
    .datain_l (shift_green[1]),
    .dataout  (tmds_d1_p)
);

ddr ddr_b (
    .outclock (clk_tmds_half),
    .datain_h (shift_blue[0]),
    .datain_l (shift_blue[1]),
    .dataout  (tmds_d0_p)
);

ddr ddr_clk (
    .outclock (clk_tmds_half),
    .datain_h (shift_clk[0]),
    .datain_l (shift_clk[1]),
    .dataout  (tmds_clk_p)
);

ddr ddr_r_n (
    .outclock (clk_tmds_half),
    .datain_h (!shift_red[0]),
    .datain_l (!shift_red[1]),
    .dataout  (tmds_d2_n)
);

ddr ddr_g_n (
    .outclock (clk_tmds_half),
    .datain_h (!shift_green[0]),
    .datain_l (!shift_green[1]),
    .dataout  (tmds_d1_n)
);

ddr ddr_b_n (
    .outclock (clk_tmds_half),
    .datain_h (!shift_blue[0]),
    .datain_l (!shift_blue[1]),
    .dataout  (tmds_d0_n)
);

ddr ddr_clk_n (
    .outclock (clk_tmds_half),
    .datain_h (!shift_clk[0]),
    .datain_l (!shift_clk[1]),
    .dataout  (tmds_clk_n)
);

endmodule
