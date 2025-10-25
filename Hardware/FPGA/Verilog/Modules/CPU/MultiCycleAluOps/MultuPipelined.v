module MultuPipelined (
    input wire clk,
    input wire reset,
    input wire [31:0] a,
    input wire [31:0] b,
    input wire start,
    output reg [63:0] y,
    output reg done
);

// Pipelined hardware multiplier
reg [31:0] a_reg1 = 32'd0;
reg [31:0] b_reg1 = 32'd0;
reg [31:0] a_reg2 = 32'd0;
reg [31:0] b_reg2 = 32'd0;
reg [63:0] partial_p_reg3 = 64'd0;
reg [63:0] final_p_reg4 = 64'd0;
reg [63:0] y_reg5 = 64'd0;

// Pipeline control - track when computation is in progress
reg [2:0] pipe_count = 3'd0;
reg pipe_active = 1'b0;

always @ (posedge clk)
begin
    if (reset)
    begin
        y <= 64'd0;
        done <= 1'b0;
        a_reg1 <= 32'd0;
        b_reg1 <= 32'd0;
        a_reg2 <= 32'd0;
        b_reg2 <= 32'd0;
        partial_p_reg3 <= 64'd0;
        final_p_reg4 <= 64'd0;
        y_reg5 <= 64'd0;
        pipe_count <= 3'd0;
        pipe_active <= 1'b0;
    end
    else
    begin
        // Default assignments
        done <= 1'b0;

        // Pipeline stage 1: Capture inputs
        a_reg1 <= a;
        b_reg1 <= b;

        // Pipeline stage 2: Register operands for DSP48 pipelining (MREG)
        a_reg2 <= a_reg1;
        b_reg2 <= b_reg1;

        // Pipeline stage 3: Perform multiplication
        partial_p_reg3 <= a_reg2 * b_reg2;

        // Pipeline stage 4: Register the multiplication result
        final_p_reg4 <= partial_p_reg3;

        // Pipeline stage 5: Register the final product
        y_reg5 <= final_p_reg4;

        y <= y_reg5;

        // Pipeline control logic
        if (start && !pipe_active)
        begin
            pipe_active <= 1'b1;
            pipe_count <= 3'd5; // 5 pipeline stages
        end
        else if (pipe_active)
        begin
            if (pipe_count > 3'd1)
            begin
                pipe_count <= pipe_count - 3'd1;
            end
            else
            begin
                done <= 1'b1;
                pipe_active <= 1'b0;
                pipe_count <= 3'd0;
            end
        end
    end
end

endmodule
