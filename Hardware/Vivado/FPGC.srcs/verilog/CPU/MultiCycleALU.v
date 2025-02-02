/*
 * Multi cycle ALU
 * Performs ARITHM instructions that require multiple cycles
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
    STATE_IDLE = 2'b00,
    STATE_CALC = 2'b01,
    STATE_DONE = 2'b10;

reg [1:0] state = STATE_IDLE;


reg [31:0] mult_a_reg = 32'd0;
reg [31:0] mult_b_reg = 32'd0;
reg [3:0] mult_opcode_reg = 4'd0;

always @ (posedge clk)
begin
    if (reset)
    begin
        done <= 1'b0;
        y <= 32'd0;
        state <= STATE_IDLE;

        mult_a_reg <= 32'd0;
        mult_b_reg <= 32'd0;
        mult_opcode_reg <= 4'd0;
    end
    else
    begin
        case (state)
            STATE_IDLE:
                begin
                    done <= 1'b0;
                    if (start)
                    begin
                        // Add an extra cycle for the synthesizer to optimize DSP usage
                        mult_a_reg <= a;
                        mult_b_reg <= b;
                        mult_opcode_reg <= opcode;
                        state <= STATE_CALC;
                    end
                end
            STATE_CALC:
                begin
                    case (mult_opcode_reg)
                        OP_MULTS:
                            begin
                                y <= $signed(mult_a_reg) * $signed(mult_b_reg);
                                done <= 1'b1;
                                state <= STATE_DONE;
                            end
                        OP_MULTU:
                            begin
                                y <= mult_a_reg * mult_b_reg;
                                done <= 1'b1;
                                state <= STATE_DONE;
                            end
                        OP_MULTFP:
                            begin
                                y <= ($signed(mult_a_reg) * $signed(mult_b_reg)) >> 16;
                                done <= 1'b1;
                                state <= STATE_DONE;
                            end
                        default:
                            state <= STATE_DONE;
                    endcase
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
