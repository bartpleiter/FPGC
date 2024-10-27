module ddr(
    input datain_h,
    input datain_l,
    input outclock,
    output wire dataout
);

assign dataout = outclock ? datain_h : datain_l;

endmodule