/*
 * Multi cycle ALU
 * Performs ARITHM instructions that require multiple cycles
 * TODO: multiple stages currently does not work for the DSP inference
 */
module MultiCycleALU (
    input wire clk,
    input wire reset,

    input wire start,
    output reg done = 1'b0,

    input wire [31:0] a,
    input wire [31:0] b,
    input wire [3:0] opcode,
    output reg [31:0] y = 32'd0
);

// Opcodes
localparam 
    OP_MULTS  = 4'b0000, // Multiply signed
    OP_MULTU  = 4'b0001, // Multiply unsigned
    OP_MULTFP = 4'b0010, // Multiply fixed point signed
    OP_DIVS   = 4'b0011, // Divide signed
    OP_DIVU   = 4'b0100, // Divide unsigned
    OP_DIVFP  = 4'b0101, // Divide fixed point signed
    OP_MODS   = 4'b0110, // Modulus signed
    OP_MODU   = 4'b0111; // Modulus unsigned

localparam
    STATE_IDLE = 3'd0,
    STATE_MULT_STAGE1 = 3'd1,
    STATE_MULT_STAGE2 = 3'd2,
    STATE_MULT_STAGE3 = 3'd3,
    STATE_DONE = 3'd4;


reg [2:0] state = STATE_IDLE;

// Extra stages are added for DSP inference
reg [3:0] mult_opcode_reg = 4'd0;
reg [31:0] mult_a_reg_stage1 = 32'd0;
reg [31:0] mult_b_reg_stage1 = 32'd0;
// reg [31:0] mult_a_reg_stage2 = 32'd0;
// reg [31:0] mult_b_reg_stage2 = 32'd0;
reg [63:0] mult_result_stage1 = 64'd0;
reg [63:0] mult_result_stage2 = 64'd0;

always @ (posedge clk)
begin
    if (reset)
    begin
        done <= 1'b0;
        y <= 32'd0;
        state <= STATE_IDLE;

        mult_opcode_reg <= 4'd0;
        mult_a_reg_stage1 <= 32'd0;
        mult_b_reg_stage1 <= 32'd0;
        // mult_a_reg_stage2 <= 32'd0;
        // mult_b_reg_stage2 <= 32'd0;
        mult_result_stage1 <= 64'd0;
        mult_result_stage2 <= 64'd0;
    end
    else
    begin
        case (state)
            STATE_IDLE:
                begin
                    done <= 1'b0;
                    if (start)
                    begin
                        mult_a_reg_stage1 <= a;
                        mult_b_reg_stage1 <= b;
                        mult_opcode_reg <= opcode;
                        state <= STATE_MULT_STAGE1;
                    end
                end
            STATE_MULT_STAGE1:
                begin
                    case (mult_opcode_reg)
                        OP_MULTS:
                            mult_result_stage1 <= $signed(mult_a_reg_stage1) * $signed(mult_b_reg_stage1);
                        OP_MULTU:
                            mult_result_stage1 <= mult_a_reg_stage1 * mult_b_reg_stage1;
                        OP_MULTFP:
                            mult_result_stage1 <= ($signed(mult_a_reg_stage1) * $signed(mult_b_reg_stage1)) >> 16;
                        default:
                            mult_result_stage1 <= 64'd0;
                    endcase
                    state <= STATE_MULT_STAGE2;
                end
            STATE_MULT_STAGE2:
                begin
                    mult_result_stage2 <= mult_result_stage1;
                    state <= STATE_MULT_STAGE3;
                end
            STATE_MULT_STAGE3:
                begin
                    y <= mult_result_stage2[31:0];
                    done <= 1'b1;
                    state <= STATE_DONE;
                end
            STATE_DONE:
                begin
                    done <= 1'b0;
                    state <= STATE_IDLE;
                end
        endcase
    end
end

endmodule
