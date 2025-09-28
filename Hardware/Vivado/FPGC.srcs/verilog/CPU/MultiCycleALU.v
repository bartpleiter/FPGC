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

wire [63:0] mults_y;
wire [63:0] multu_y;

localparam
    STATE_IDLE = 3'd0,
    STATE_MULT_STAGE1 = 3'd1,
    STATE_MULT_STAGE2 = 3'd2,
    STATE_MULT_STAGE3 = 3'd3,
    STATE_MULT_STAGE4 = 3'd4,
    STATE_MULT_STAGE5 = 3'd5,
    STATE_FETCH_RESULT = 3'd6,
    STATE_DONE = 3'd7;


reg [2:0] state = STATE_IDLE;

always @ (posedge clk)
begin
    if (reset)
    begin
        done <= 1'b0;
        state <= STATE_IDLE;
        y <= 32'd0;
    end
    else
    begin
        case (state)
            STATE_IDLE:
                begin
                    done <= 1'b0;
                    if (start)
                    begin
                        state <= STATE_MULT_STAGE1;
                    end
                end
            STATE_MULT_STAGE1:
                begin
                    state <= STATE_MULT_STAGE2;
                end
            STATE_MULT_STAGE2:
                begin
                    state <= STATE_MULT_STAGE3;
                end
            STATE_MULT_STAGE3:
                begin
                    state <= STATE_MULT_STAGE4;
                end
            STATE_MULT_STAGE4:
                begin
                    state <= STATE_MULT_STAGE5;
                end
            STATE_MULT_STAGE5:
                begin
                    state <= STATE_FETCH_RESULT;
                end
            STATE_FETCH_RESULT:
                begin
                    case (opcode)
                        OP_MULTS:
                            y <= multu_y[31:0];
                        OP_MULTU:
                            y <= mults_y[31:0];
                        OP_MULTFP:
                            y <= mults_y[47:16];
                        default:
                            y <= 32'd0;
                    endcase
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

MultuPipelined multu (
    .clk(clk),
    .reset(reset),
    .a(a),
    .b(b),
    .y(multu_y)
);

MultsPipelined mults (
    .clk(clk),
    .reset(reset),
    .a(a),
    .b(b),
    .y(mults_y)
);

endmodule
