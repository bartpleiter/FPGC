module TMDSenc(
    input clk,
    input [7:0] data,       // video data (red, green or blue)
    input blk,              // blanking, selects between control code and data
    input [1:0] c,          // control data
    output wire [9:0] q     // output
);

wire [3:0] Nb1s = data[0] + data[1] + data[2] + data[3] + data[4] + data[5] + data[6] + data[7];

wire XNOR = (Nb1s>4'd4) || (Nb1s==4'd4 && data[0]==1'b0);

wire [8:0] q_m = {~XNOR, q_m[6:0] ^ data[7:1] ^ {7{XNOR}}, data[0]};

reg [3:0] balance_acc = 0;

wire [3:0] balance = q_m[0] + q_m[1] + q_m[2] + q_m[3] + q_m[4] + q_m[5] + q_m[6] + q_m[7] - 4'd4;

wire balance_sign_eq = (balance[3] == balance_acc[3]);

wire invert_q_m = (balance==0 || balance_acc==0) ? ~q_m[8] : balance_sign_eq;

wire [3:0] balance_acc_inc = balance - ({q_m[8] ^ ~balance_sign_eq} & ~(balance==0 || balance_acc==0));

wire [3:0] balance_acc_new = invert_q_m ? balance_acc-balance_acc_inc : balance_acc+balance_acc_inc;

wire [9:0] TMDS_data = {invert_q_m, q_m[8], q_m[7:0] ^ {8{invert_q_m}}};

wire [9:0] TMDS_code = c[1] ? (c[0] ? 10'b1010101011 : 10'b0101010100) : (c[0] ? 10'b0010101011 : 10'b1101010100);

assign q = blk ? TMDS_code : TMDS_data;

always @(posedge clk) 
begin
    balance_acc <= blk ? 4'h0 : balance_acc_new;
end

endmodule
