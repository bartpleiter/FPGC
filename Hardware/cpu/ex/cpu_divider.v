// Divider
// Result stays at y until the next start.
module cpu_divider (
    input  wire        clk,
    input  wire        start,
    input  wire [3:0]  op,      // the divide or modulo operation
    input  wire [31:0] a,       // the dividend
    input  wire [31:0] b,       // the divisor
    output wire        busy,
    output wire [31:0] y
);

wire is_signed = op == 4'b0011 || op == 4'b0101 || op == 4'b0110;  // divs, divfp, mods
wire is_modulo = op == 4'b0110 || op == 4'b0111;                   // mods, modu
wire is_fp     = op == 4'b0101;                                    // divfp

wire        a_negative = is_signed && a[31];
wire        b_negative = is_signed && b[31];
wire [31:0] a_magnitude = a_negative ? -a : a;
wire [31:0] b_magnitude = b_negative ? -b : b;

reg [47:0] quotient;
reg [31:0] remainder;
reg [31:0] divisor;
reg [5:0]  steps;
reg        modulo;
reg        negate;

wire [32:0] shifted = {remainder, quotient[47]};
wire        fits    = shifted >= {1'b0, divisor};

always @(posedge clk) begin
    if (start) begin
        quotient  <= {a_magnitude, 16'd0};
        remainder <= 32'd0;
        divisor   <= b_magnitude;
        steps     <= is_fp ? 6'd48 : 6'd32;
        modulo    <= is_modulo;
        negate    <= is_modulo ? a_negative : a_negative != b_negative && b != 32'd0;
    end else if (steps != 6'd0) begin
        quotient  <= {quotient[46:0], fits};
        remainder <= fits ? shifted[31:0] - divisor : shifted[31:0];
        steps     <= steps - 6'd1;
    end
end

assign busy = start || steps != 6'd0;

wire [31:0] magnitude = modulo ? remainder : quotient[31:0];

assign y = negate ? -magnitude : magnitude;

endmodule
