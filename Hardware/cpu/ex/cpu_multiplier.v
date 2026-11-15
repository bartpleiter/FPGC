// Multiplier
// A pipelined 32 x 32 multiply in two stages.
// Optimized for Cyclone IV hardware multipliers, which is why the multiply is split into 16-bit halves.
// Supports fixed point multiplication (Q16.16).
// A new multiply can start every cycle.
module cpu_multiplier (
    input  wire        clk,
    input  wire        enable,
    input  wire [3:0]  op,      // the multiply operation
    input  wire [31:0] a,
    input  wire [31:0] b,
    output wire [31:0] y        // the result of the operands taken last
);

// Works on the principle that a x b = ah*bh * 2^32 + (ah*bl + al*bh) * 2^16 + al*bl.
wire is_signed = op == 4'b0000 || op == 4'b0010 || op == 4'b1110;  // mults, multfp, mulshi

wire signed [16:0] ah = {is_signed && a[31], a[31:16]};
wire signed [16:0] al = {1'b0, a[15:0]};
wire signed [16:0] bh = {is_signed && b[31], b[31:16]};
wire signed [16:0] bl = {1'b0, b[15:0]};

reg        [31:0] hh;
reg signed [33:0] hl;
reg signed [33:0] lh;
reg        [31:0] ll;
reg        [1:0]  select;

localparam LOW  = 2'd0;
localparam FP   = 2'd1;
localparam HIGH = 2'd2;

always @(posedge clk) begin
    if (enable) begin
        hh     <= ah * bh;
        hl     <= ah * bl;
        lh     <= al * bh;
        ll     <= a[15:0] * b[15:0];
        select <= op == 4'b0010 ? FP : op[3] ? HIGH : LOW;
    end
end

wire signed [34:0] middle  = hl + lh;
wire        [63:0] product = {hh, ll} + {{13{middle[34]}}, middle, 16'd0};

assign y = select == HIGH ? product[63:32] :
           select == FP   ? product[47:16] :
                            product[31:0];

endmodule
