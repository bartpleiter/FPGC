// Multiplier
// A pipelined 32 x 32 multiply in two stages.
// A new multiply can start every cycle.
module cpu_multiplier (
    input  wire        clk,
    input  wire        enable,
    input  wire [3:0]  op,      // the multiply operation
    input  wire [31:0] a,
    input  wire [31:0] b,
    output wire [31:0] y        // the result of the operands taken last
);

// The operands are split in 16-bit halves, so a x b is
// ah*bh * 2^32 + (ah*bl + al*bh) * 2^16 + al*bl. The high halves are signed for the
// signed operations and the low halves are always unsigned, so each half gets a 17th
// bit and each part is a 17 x 17 signed multiply, which fits a DSP block of the FPGA.
// The low 32 bits of the product are the same for a signed and an unsigned multiply.
wire is_signed = op == 4'b0000 || op == 4'b0010 || op == 4'b1110;  // mults, multfp, mulshi

wire signed [16:0] ah = {is_signed && a[31], a[31:16]};
wire signed [16:0] al = {1'b0, a[15:0]};
wire signed [16:0] bh = {is_signed && b[31], b[31:16]};
wire signed [16:0] bl = {1'b0, b[15:0]};

// al*bl fits in 32 bits, and of ah*bh only the low 32 bits matter, because it starts
// at bit 32 of the product.
reg        [31:0] hh;
reg signed [33:0] hl;
reg signed [33:0] lh;
reg        [31:0] ll;
reg        [1:0]  select;

localparam LOW  = 2'd0;  // mults, multu: bits 31 to 0
localparam FP   = 2'd1;  // multfp: bits 47 to 16, a fixed-point multiply with 16 fraction bits
localparam HIGH = 2'd2;  // mulshi, multuhi: bits 63 to 32

always @(posedge clk) begin
    if (enable) begin
        hh     <= ah * bh;
        hl     <= ah * bl;
        lh     <= al * bh;
        ll     <= a[15:0] * b[15:0];
        select <= op == 4'b0010 ? FP : op[3] ? HIGH : LOW;
    end
end

// al*bl fills bits 31 to 0, and ah*bh starts at bit 32, so those two only need to be
// put side by side. The two middle parts are added and shifted in.
wire signed [34:0] middle  = hl + lh;
wire        [63:0] product = {hh, ll} + {{13{middle[34]}}, middle, 16'd0};

assign y = select == HIGH ? product[63:32] :
           select == FP   ? product[47:16] :
                            product[31:0];

endmodule
