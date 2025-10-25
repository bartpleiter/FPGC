/*
 * Multi cycle ALU
 * Performs ARITHM instructions that require multiple cycles
 * Uses a state machine to orchestrate the operations to the relevant submodules based on opcode
 * While this does add latency, it simplifies the design and makes it behave similar to the Memory Unit
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

// State machine states
localparam
    STATE_IDLE = 4'd0,
    STATE_WAIT_MULTU = 4'd1,
    STATE_WAIT_MULTS = 4'd2,
    STATE_WAIT_MULTFP = 4'd3,
    STATE_DONE = 4'd15;

reg [3:0] state = STATE_IDLE;

// Multicycle operations submodules
reg [31:0] multu_a = 32'd0;
reg [31:0] multu_b = 32'd0;
wire [63:0] multu_y;
reg multu_start = 1'b0;
wire multu_done;
MultuPipelined multu (
    .clk(clk),
    .reset(reset),
    .a(multu_a),
    .b(multu_b),
    .y(multu_y),
    .start(multu_start),
    .done(multu_done)
);

reg [31:0] mults_a = 32'd0;
reg [31:0] mults_b = 32'd0;
wire [63:0] mults_y;
reg mults_start = 1'b0;
wire mults_done;
MultsPipelined mults (
    .clk(clk),
    .reset(reset),
    .a(mults_a),
    .b(mults_b),
    .y(mults_y),
    .start(mults_start),
    .done(mults_done)
);

always @ (posedge clk)
begin
    if (reset)
    begin
        done <= 1'b0;
        state <= STATE_IDLE;
        y <= 32'd0;

        // Reset submodule signals
        multu_start <= 1'b0;
        multu_a <= 32'd0;
        multu_b <= 32'd0;
        mults_start <= 1'b0;
        mults_a <= 32'd0;
        mults_b <= 32'd0;
    end
    else
    begin
        // Default assignments
        done <= 1'b0;
        y <= y;
        multu_start <= 1'b0;
        mults_start <= 1'b0;
        case (state)
            STATE_IDLE:
            begin
                if (start)
                begin
                    if (opcode == OP_MULTU)
                    begin
                        multu_a <= a;
                        multu_b <= b;
                        multu_start <= 1'b1;
                        state <= STATE_WAIT_MULTU;
                    end

                    if (opcode == OP_MULTS)
                    begin
                        mults_a <= a;
                        mults_b <= b;
                        mults_start <= 1'b1;
                        state <= STATE_WAIT_MULTS;
                    end

                    if (opcode == OP_MULTFP)
                    begin
                        mults_a <= a;
                        mults_b <= b;
                        mults_start <= 1'b1;
                        state <= STATE_WAIT_MULTFP;
                    end
                end
            end

            STATE_WAIT_MULTU:
            begin
                if (multu_done)
                begin
                    y <= multu_y[31:0];
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_MULTS:
            begin
                if (mults_done)
                begin
                    y <= mults_y[31:0];
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_MULTFP:
            begin
                if (mults_done)
                begin
                    y <= mults_y[47:16]; // Fixed point result adjustment
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            default:
                state <= STATE_IDLE;
        endcase
    end
end

endmodule
