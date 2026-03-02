/*
 * Mults64
 * Registered signed 64x64 multiplier for FP64 coprocessor
 * Produces a 128-bit signed product in 3 clock cycles
 * Synthesis maps the 64-bit multiply to DSP block cascades automatically
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

reg signed [63:0] areg = 64'd0;
reg signed [63:0] breg = 64'd0;
reg done_pipe1 = 1'b0;
reg done_pipe2 = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        y <= 128'd0;
        done <= 1'b0;
        areg <= 64'd0;
        breg <= 64'd0;
        done_pipe1 <= 1'b0;
        done_pipe2 <= 1'b0;
    end
    else
    begin
        // Stage 1: Register inputs
        areg <= a;
        breg <= b;
        // Stage 2: Multiply (synthesis maps to DSP cascade)
        y <= areg * breg;
        // Stage 3: Done tracking (3-cycle pipeline)
        done_pipe1 <= start;
        done_pipe2 <= done_pipe1;
        done <= done_pipe2;
    end
end

endmodule
