module lvds(
    input [3:0] datain,
    output wire [3:0] dataout,
    output wire [3:0] dataout_b
);

assign dataout = datain;
assign dataout_b = ~datain;

endmodule