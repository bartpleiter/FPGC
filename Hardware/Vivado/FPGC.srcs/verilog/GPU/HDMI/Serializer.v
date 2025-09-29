/*
 * HDMI Serializer with DDR clocking
 */
module Serializer (
    // Clocks
    input wire clkTMDShalf,
    input wire clkRGB,

    // TMDS
    input wire [9:0] TMDS_data,
    output wire      TMDS_out_p,
    output wire      TMDS_out_n
);

// Reset is needed for the OSERDESE2
reg internal_reset = 1'b1;
always @(posedge clkRGB)
begin
    internal_reset <= 1'b0;
end

// Two OSERDESE2 in master and slave configuration
wire tmds_out;
wire cascade_di;
wire cascade_ti;
OSERDESE2 #(
    .DATA_RATE_OQ("DDR"),       // Double Data Rate (DDR)
    .DATA_RATE_TQ("SDR"),       // Single Data Rate for tri-state
    .SERDES_MODE("MASTER"),     // Master mode
    .TRISTATE_WIDTH(1),         // Tri-state control width
    .DATA_WIDTH(10),            // 10-bit serialization
    .TBYTE_CTL("FALSE"),
    .TBYTE_SRC("FALSE")
) oserdes_master (
    .OQ(tmds_out),              // Serialized output
    .OFB(),
    .TQ(),
    .TFB(),
    .SHIFTIN1(cascade_di),
    .SHIFTIN2(cascade_ti),
    .SHIFTOUT1(),
    .SHIFTOUT2(),
    .TBYTEOUT(),
    .CLK(clkTMDShalf),          // 5x Pixel clock
    .CLKDIV(clkRGB),            // Pixel clock
    .D1(TMDS_data[0]),
    .D2(TMDS_data[1]),
    .D3(TMDS_data[2]),
    .D4(TMDS_data[3]),
    .D5(TMDS_data[4]),
    .D6(TMDS_data[5]),
    .D7(TMDS_data[6]),
    .D8(TMDS_data[7]),
    .TCE(1'b0),
    .TBYTEIN(1'b0),
    .OCE(1'b1),                 // Output clock enable
    .RST(internal_reset),
    .T1(1'b0),
    .T2(1'b0),
    .T3(1'b0),
    .T4(1'b0)
);

OSERDESE2 #(
    .DATA_RATE_OQ("DDR"),       // Double Data Rate (DDR)
    .DATA_RATE_TQ("SDR"),       // Single Data Rate for tri-state
    .SERDES_MODE("SLAVE"),      // Slave mode
    .TRISTATE_WIDTH(1),         // Tri-state control width
    .DATA_WIDTH(10),            // 10-bit serialization
    .TBYTE_CTL("FALSE"),
    .TBYTE_SRC("FALSE")
) oserdes_slave (
    .OQ(),
    .OFB(),
    .TQ(),
    .TFB(),
    .SHIFTIN1(1'b0),
    .SHIFTIN2(1'b0),
    .SHIFTOUT1(cascade_di),
    .SHIFTOUT2(cascade_ti),
    .TBYTEOUT(),
    .CLK(clkTMDShalf),          // 5x Pixel clock
    .CLKDIV(clkRGB),            // Pixel clock
    .D1(1'b0),
    .D2(1'b0),
    .D3(TMDS_data[8]),
    .D4(TMDS_data[9]),
    .D5(1'b0),
    .D6(1'b0),
    .D7(1'b0),
    .D8(1'b0),
    .TCE(1'b0),
    .TBYTEIN(1'b0),
    .OCE(1'b1),                 // Output clock enable
    .RST(internal_reset),
    .T1(1'b0),
    .T2(1'b0),
    .T3(1'b0),
    .T4(1'b0)
);

// Differential output
OBUFDS obufds_data (
    .I(tmds_out),
    .O(TMDS_out_p),
    .OB(TMDS_out_n)
);

endmodule
