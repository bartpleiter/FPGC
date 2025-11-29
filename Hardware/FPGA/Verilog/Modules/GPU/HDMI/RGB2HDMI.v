/*
 * RGB2HDMI
 * VGA RGBHS to HDMI signal converter
 */
module RGB2HDMI (
    // Clocks
    input wire          clkTMDShalf,
    input wire          clkRGB,

    // RGB
    input wire  [7:0]   rRGB,
    input wire  [7:0]   gRGB,
    input wire  [7:0]   bRGB,
    input wire          blk,
    input wire          hs,
    input wire          vs,

    // HDMI
    output wire         TMDS_clk_p,
    output wire         TMDS_clk_n,
    output wire         TMDS_d0_p,
    output wire         TMDS_d0_n,
    output wire         TMDS_d1_p,
    output wire         TMDS_d1_n,
    output wire         TMDS_d2_p,
    output wire         TMDS_d2_n
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

// Serializer logic for Cyclone FPGAs
`ifdef CYCLONE

    reg [9:0] latchedRed    = 10'd0;
    reg [9:0] latchedGreen  = 10'd0;
    reg [9:0] latchedBlue   = 10'd0;

    reg [9:0] shiftRed      = 10'd0;
    reg [9:0] shiftGreen    = 10'd0;
    reg [9:0] shiftBlue     = 10'd0;

    reg [9:0] shiftClk      = 10'b0000011111;

    always @(posedge clkRGB)
    begin
        latchedRed   <= encodedRed;
        latchedGreen <= encodedGreen;
        latchedBlue  <= encodedBlue;
    end

    always @(posedge clkTMDShalf)
    begin
        if (shiftClk == 10'b0000011111)
        begin
            shiftRed   <= latchedRed;
            shiftGreen <= latchedGreen;
            shiftBlue  <= latchedBlue;
        end
        else
        begin
            shiftRed   <= {2'b00, shiftRed[9:2]};
            shiftGreen <= {2'b00, shiftGreen[9:2]};
            shiftBlue  <= {2'b00, shiftBlue[9:2]};
        end
            shiftClk <= {shiftClk[1:0], shiftClk[9:2]};
    end

    // DDR each signal to double clock rate
    ddr ddrR(
        .outclock(clkTMDShalf),
        .datain_h(shiftRed[0]),
        .datain_l(shiftRed[1]),
        .dataout (TMDS_d2_p)
    );

    ddr ddrG(
        .outclock(clkTMDShalf),
        .datain_h(shiftGreen[0]),
        .datain_l(shiftGreen[1]),
        .dataout (TMDS_d1_p)
    );

    ddr ddrB(
        .outclock(clkTMDShalf),
        .datain_h(shiftBlue[0]),
        .datain_l(shiftBlue[1]),
        .dataout (TMDS_d0_p)
    );

    ddr ddrCLK(
        .outclock(clkTMDShalf),
        .datain_h(shiftClk[0]),
        .datain_l(shiftClk[1]),
        .dataout (TMDS_clk_p)
    );

    ddr ddrRn(
        .outclock(clkTMDShalf),
        .datain_h(!shiftRed[0]),
        .datain_l(!shiftRed[1]),
        .dataout (TMDS_d2_n)
    );

    ddr ddrGn(
        .outclock(clkTMDShalf),
        .datain_h(!shiftGreen[0]),
        .datain_l(!shiftGreen[1]),
        .dataout (TMDS_d1_n)
    );

    ddr ddrBn(
        .outclock(clkTMDShalf),
        .datain_h(!shiftBlue[0]),
        .datain_l(!shiftBlue[1]),
        .dataout (TMDS_d0_n)
    );

    ddr ddrCLKn(
        .outclock(clkTMDShalf),
        .datain_h(!shiftClk[0]),
        .datain_l(!shiftClk[1]),
        .dataout (TMDS_clk_n)
    );

// Serializer logic for Artix 7 board
`else
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
`endif

endmodule
