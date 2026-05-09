/*
 * Mults64
 * Pipelined signed 64x64 multiplier for FP64 coprocessor
 * Produces a 128-bit signed product in 4 clock cycles
 *
 * Decomposes into four parallel 32x32 multiplies (each fits comfortably
 * in DSP blocks with short carry chains) followed by a registered
 * accumulation stage. This breaks the critical ~13ns carry chain of a
 * single-cycle 64x64 multiply into two stages of ~7-8ns each.
 *
 * Pipeline:
 *   Stage 1: Register inputs
 *   Stage 2: Four parallel 32x32 multiplies, register partial products
 *   Stage 3: Accumulate shifted partial products, register result
 *   Stage 4: Done
 */
module Mults64 (
    input  wire                clk,
    input  wire                reset,

    input  wire signed [63:0]  a,
    input  wire signed [63:0]  b,
    input  wire                start,

    output reg  signed [127:0] y,
    output reg                 done
);

// ---- Stage 1: Register inputs ----
reg signed [63:0] areg = 64'd0;
reg signed [63:0] breg = 64'd0;
reg s1_valid = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        areg <= 64'd0;
        breg <= 64'd0;
        s1_valid <= 1'b0;
    end
    else
    begin
        areg <= a;
        breg <= b;
        s1_valid <= start;
    end
end

// ---- Stage 2: Four parallel 32x32 multiplies ----
// Decompose: a = a_hi*2^32 + a_lo, b = b_hi*2^32 + b_lo
// where a_hi, b_hi are signed 32-bit, a_lo, b_lo are unsigned 32-bit
//
// a*b = (a_hi*b_hi)*2^64 + (a_hi*b_lo + a_lo*b_hi)*2^32 + a_lo*b_lo

wire signed [31:0] a_hi = areg[63:32];
wire        [31:0] a_lo = areg[31:0];
wire signed [31:0] b_hi = breg[63:32];
wire        [31:0] b_lo = breg[31:0];

reg signed [63:0] pp_hh = 64'd0;  // a_hi * b_hi (signed x signed)
reg signed [63:0] pp_hl = 64'd0;  // a_hi * b_lo (signed x unsigned)
reg signed [63:0] pp_lh = 64'd0;  // a_lo * b_hi (unsigned x signed)
reg        [63:0] pp_ll = 64'd0;  // a_lo * b_lo (unsigned x unsigned)
reg s2_valid = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        pp_hh <= 64'd0;
        pp_hl <= 64'd0;
        pp_lh <= 64'd0;
        pp_ll <= 64'd0;
        s2_valid <= 1'b0;
    end
    else
    begin
        pp_hh <= a_hi * b_hi;
        pp_hl <= a_hi * $signed({1'b0, b_lo});
        pp_lh <= $signed({1'b0, a_lo}) * b_hi;
        pp_ll <= a_lo * b_lo;
        s2_valid <= s1_valid;
    end
end

// ---- Stage 3: Accumulate partial products ----
// result = {pp_hh, 64'd0}
//        + {sign_ext(pp_hl), 32'd0}
//        + {sign_ext(pp_lh), 32'd0}
//        + {64'd0, pp_ll}

wire [127:0] accum = {pp_hh, 64'd0}
                   + {{32{pp_hl[63]}}, pp_hl, 32'd0}
                   + {{32{pp_lh[63]}}, pp_lh, 32'd0}
                   + {64'd0, pp_ll};

reg s3_valid = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        y <= 128'd0;
        s3_valid <= 1'b0;
    end
    else
    begin
        y <= accum;
        s3_valid <= s2_valid;
    end
end

// ---- Stage 4: Done ----
always @(posedge clk)
begin
    if (reset)
        done <= 1'b0;
    else
        done <= s3_valid;
end

endmodule
