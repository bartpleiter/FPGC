/*
* Stabilizes multiple input signals (up to 16) to the clock domain,
* by using a double register synchronizer chain.
*/
module MultiStabilizer (
    input clk,

    // unstable input signals
    input u0, u1, u2, u3, u4, u5, u6, u7, u8, u9, u10, u11, u12, u13, u14, u15,
    
    // stable output signals
    output s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15
);

    // chains
    reg [1:0] c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15;
    always @(posedge clk) 
    begin
        c0[0] <= u0;
        c0[1] <= c0[0];

        c1[0] <= u1;
        c1[1] <= c1[0];

        c2[0] <= u2;
        c2[1] <= c2[0];

        c3[0] <= u3;
        c3[1] <= c3[0];

        c4[0] <= u4;
        c4[1] <= c4[0];

        c5[0] <= u5;
        c5[1] <= c5[0];

        c6[0] <= u6;
        c6[1] <= c6[0];

        c7[0] <= u7;
        c7[1] <= c7[0];

        c8[0] <= u8;
        c8[1] <= c8[0];

        c9[0] <= u9;
        c9[1] <= c9[0];

        c10[0] <= u10;
        c10[1] <= c10[0];

        c11[0] <= u11;
        c11[1] <= c11[0];

        c12[0] <= u12;
        c12[1] <= c12[0];

        c13[0] <= u13;
        c13[1] <= c13[0];

        c14[0] <= u14;
        c14[1] <= c14[0];

        c15[0] <= u15;
        c15[1] <= c15[0];
    end

    assign s0 = c0[1];
    assign s1 = c1[1];
    assign s2 = c2[1];
    assign s3 = c3[1];
    assign s4 = c4[1];
    assign s5 = c5[1];
    assign s6 = c6[1];
    assign s7 = c7[1];
    assign s8 = c8[1];
    assign s9 = c9[1];
    assign s10 = c10[1];
    assign s11 = c11[1];
    assign s12 = c12[1];
    assign s13 = c13[1];
    assign s14 = c14[1];
    assign s15 = c15[1];

endmodule