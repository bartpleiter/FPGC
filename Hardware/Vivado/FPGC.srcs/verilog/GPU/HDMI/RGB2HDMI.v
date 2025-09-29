/*
 * VGA RGBHS to HDMI signal converter
 */
module RGB2HDMI (
    // Clocks
    input wire clkTMDShalf,
    input wire clkRGB,

    // RGB
    input wire [7:0] rRGB,
    input wire [7:0] gRGB,
    input wire [7:0] bRGB,
    input wire blk,
    input wire hs,
    input wire vs,

    // HDMI
    output wire TMDS_clk_p,
    output wire TMDS_clk_n,
    output wire TMDS_d0_p,
    output wire TMDS_d0_n,
    output wire TMDS_d1_p,
    output wire TMDS_d1_n,
    output wire TMDS_d2_p,
    output wire TMDS_d2_n
);

wire [9:0] encodedRed;
wire [9:0] encodedGreen;
wire [9:0] encodedBlue;

TMDSenc TMDSr (
    .clk (clkRGB),
    .data(rRGB),
    .c   (2'd0),
    .blk (blk),
    .q   (encodedRed)
);

TMDSenc TMDSg (
    .clk (clkRGB),
    .data(gRGB),
    .c   (2'd0),
    .blk (blk),
    .q   (encodedGreen)
);

TMDSenc TMDSb (
    .clk (clkRGB),
    .data(bRGB),
    .c   ({vs, hs}),
    .blk (blk),
    .q   (encodedBlue)
);

Serializer tmds_clk_serializer (
    .clkTMDShalf(clkTMDShalf),
    .clkRGB(clkRGB),
    .TMDS_data(10'b1111100000),
    .TMDS_out_p(TMDS_clk_p),
    .TMDS_out_n(TMDS_clk_n)
);

Serializer tmds_d0_serializer (
    .clkTMDShalf(clkTMDShalf),
    .clkRGB(clkRGB),
    .TMDS_data(encodedRed),
    .TMDS_out_p(TMDS_d2_p),
    .TMDS_out_n(TMDS_d2_n)
);

Serializer tmds_d1_serializer (
    .clkTMDShalf(clkTMDShalf),
    .clkRGB(clkRGB),
    .TMDS_data(encodedGreen),
    .TMDS_out_p(TMDS_d1_p),
    .TMDS_out_n(TMDS_d1_n)
);

Serializer tmds_d2_serializer (
    .clkTMDShalf(clkTMDShalf),
    .clkRGB(clkRGB),
    .TMDS_data(encodedBlue),
    .TMDS_out_p(TMDS_d0_p),
    .TMDS_out_n(TMDS_d0_n)
);

endmodule
