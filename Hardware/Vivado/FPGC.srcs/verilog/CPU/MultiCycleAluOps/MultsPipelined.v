module MultsPipelined (
    input wire clk,
    input wire reset,
    input wire signed [31:0] a,
    input wire signed [31:0] b,
    output reg [63:0] y
);

// Pipelined hardware multiplier
reg signed [31:0] a_reg1 = 32'd0;
reg signed [31:0] b_reg1 = 32'd0;
reg signed [31:0] a_reg2 = 32'd0;
reg signed [31:0] b_reg2 = 32'd0;
reg [63:0] partial_p_reg3 = 64'd0;
reg [63:0] final_p_reg4 = 64'd0;
reg [63:0] y_reg5 = 64'd0;

always @ (posedge clk)
begin
    if (reset)
    begin
        y <= 64'd0;
        a_reg1 <= 32'd0;
        b_reg1 <= 32'd0;
        a_reg2 <= 32'd0;
        b_reg2 <= 32'd0;
        partial_p_reg3 <= 64'd0;
        final_p_reg4 <= 64'd0;
        y_reg5 <= 64'd0;
    end
    else
    begin
        // Pipeline stage 1: Capture inputs
        a_reg1 <= a;
        b_reg1 <= b;

        // Pipeline stage 2: Register operands for DSP48 pipelining
        a_reg2 <= a_reg1;
        b_reg2 <= b_reg1;

        // Pipeline stage 3: Perform multiplication
        partial_p_reg3 <= a_reg2 * b_reg2;

        // Pipeline stage 4: Register the multiplication result
        final_p_reg4 <= partial_p_reg3;

        // Pipeline stage 5: Register the final product
        y_reg5 <= final_p_reg4;

        // Output the final registered product
        y <= y_reg5;
    end
end

endmodule
