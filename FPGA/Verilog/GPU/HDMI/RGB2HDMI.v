module RGB2HDMI(
    input clkTMDS,
    input clkRGB,
    input [7:0] rRGB,
    input [7:0] gRGB,
    input [7:0] bRGB,
    input blk,
    input hs,
    input vs,
    output wire rTMDS,
    output wire gTMDS,
    output wire bTMDS,
    output wire cTMDS
);

wire [9:0] encodedRed;
wire [9:0] encodedGreen;
wire [9:0] encodedBlue;

reg [9:0] latchedRed    = 10'd0;
reg [9:0] latchedGreen  = 10'd0;
reg [9:0] latchedBlue   = 10'd0;

reg [9:0] shiftRed      = 10'd0;
reg [9:0] shiftGreen    = 10'd0;
reg [9:0] shiftBlue     = 10'd0;

reg [9:0] shiftClk      = 10'b0000011111;


TMDSenc TMDSr(
    .clk (clkRGB),
    .data(rRGB),
    .c   (2'd0),
    .blk (blk),
    .q   (encodedRed)
);

TMDSenc TMDSg(
    .clk (clkRGB),
    .data(gRGB),
    .c   (2'd0),
    .blk (blk),
    .q   (encodedGreen)
);

TMDSenc TMDSb(
    .clk (clkRGB),
    .data(bRGB),
    .c   ({vs, hs}),
    .blk (blk),
    .q   (encodedBlue)
);


// Everything is inverted here because of a LVDS polarity swap on the V3 PCB
ddr ddrR(
    .outclock(clkTMDS),
    .datain_h(!shiftRed[0]),
    .datain_l(!shiftRed[1]),
    .dataout (rTMDS)
);

ddr ddrG(
    .outclock(clkTMDS),
    .datain_h(!shiftGreen[0]),
    .datain_l(!shiftGreen[1]),
    .dataout (gTMDS)
);

ddr ddrB(
    .outclock(clkTMDS),
    .datain_h(!shiftBlue[0]),
    .datain_l(!shiftBlue[1]),
    .dataout (bTMDS)
);

ddr ddrCLK(
    .outclock(clkTMDS),
    .datain_h(!shiftClk[0]),
    .datain_l(!shiftClk[1]),
    .dataout (cTMDS)
);


always @(posedge clkRGB)
begin
    latchedRed   <= encodedRed;
    latchedGreen <= encodedGreen;
    latchedBlue  <= encodedBlue;
end


always @(posedge clkTMDS)
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

endmodule
