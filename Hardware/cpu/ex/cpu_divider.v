// Divider
// An iterative divide that makes one bit of the quotient per cycle.
// The result stays at y until the next start.
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

// The quotient register starts with the dividend, and every step shifts one bit of the
// dividend out at the top into the remainder, and one bit of the quotient in at the
// bottom. The dividend starts at bit 47, so after 32 steps, bits 31 to 0 are the
// quotient. divfp takes 16 more steps, which shifts in 16 more zeros: that divides
// a * 2^16, and leaves the low 32 bits of that quotient in bits 31 to 0.
reg [47:0] quotient;
reg [31:0] remainder;
reg [31:0] divisor;
reg [5:0]  steps;    // the steps still to do
reg        modulo;   // the result is the remainder
reg        negate;   // the result is negative

// One step: take the next bit of the dividend, and subtract the divisor if it fits.
// What's left is less than the divisor, so it fits in 32 bits.
wire [32:0] shifted = {remainder, quotient[47]};
wire        fits    = shifted >= {1'b0, divisor};

always @(posedge clk) begin
    if (start) begin
        quotient  <= {a_magnitude, 16'd0};
        remainder <= 32'd0;
        divisor   <= b_magnitude;
        steps     <= is_fp ? 6'd48 : 6'd32;
        modulo    <= is_modulo;
        // The quotient is negative when the signs differ, and the remainder has the
        // sign of the dividend. A division by zero gives all ones as the quotient,
        // without a sign, and the dividend as the remainder.
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
