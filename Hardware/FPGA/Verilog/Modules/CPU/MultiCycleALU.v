/*
 * MultiCycleALU
 * Performs ARITHM instructions that require multiple cycles
 * Uses a state machine to orchestrate the operations to the relevant submodules based on opcode
 * While this does add latency, it simplifies the design and makes it behave similar to the Memory Unit
 */
module MultiCycleALU (
    input wire          clk,
    input wire          reset,

    input wire          start,
    output reg          done = 1'b0,

    input wire  [31:0]  a,
    input wire  [31:0]  b,
    input wire  [3:0]   opcode,
    output reg  [31:0]  y = 32'd0,

    // FP64 coprocessor ports for FMUL
    input wire  [63:0]  fp_a,        // 64-bit FP operand A (from FP register file)
    input wire  [63:0]  fp_b,        // 64-bit FP operand B (from FP register file)
    output reg  [63:0]  fp_result = 64'd0  // 64-bit FP result (32.32 fixed-point)
);

// Opcodes
localparam
    OP_MULTS    = 4'b0000, // Multiply signed
    OP_MULTU    = 4'b0001, // Multiply unsigned
    OP_MULTFP   = 4'b0010, // Multiply fixed point signed
    OP_DIVS     = 4'b0011, // Divide signed
    OP_DIVU     = 4'b0100, // Divide unsigned
    OP_DIVFP    = 4'b0101, // Divide fixed point signed
    OP_MODS     = 4'b0110, // Modulus signed
    OP_MODU     = 4'b0111, // Modulus unsigned
    // FP64 coprocessor opcodes
    OP_FMUL     = 4'b1000, // 64-bit fixed-point multiply (uses Mults64)
    OP_FADD     = 4'b1001, // 64-bit add (single-cycle, handled in B32P3.v)
    OP_FSUB     = 4'b1010, // 64-bit sub (single-cycle, handled in B32P3.v)
    OP_FLD      = 4'b1011, // Load FP reg (single-cycle, handled in B32P3.v)
    OP_FSTHI    = 4'b1100, // Store FP reg high (single-cycle, handled in B32P3.v)
    OP_FSTLO    = 4'b1101, // Store FP reg low (single-cycle, handled in B32P3.v)
    OP_MULSHI   = 4'b1110, // Multiply signed, return high 32 bits
    OP_MULTUHI  = 4'b1111; // Multiply unsigned, return high 32 bits

// State machine states
localparam
    STATE_IDLE          = 4'd0,
    STATE_WAIT_MULTU    = 4'd1,
    STATE_WAIT_MULTS    = 4'd2,
    STATE_WAIT_MULTFP   = 4'd3,
    STATE_WAIT_IDIV     = 4'd4,
    STATE_WAIT_FPDIV    = 4'd5,
    STATE_WAIT_MULSHI   = 4'd6,
    STATE_WAIT_MULTUHI  = 4'd7,
    STATE_WAIT_MULTS64  = 4'd8;

reg [3:0] state = STATE_IDLE;

// Track whether we want quotient or remainder for integer division
reg idiv_want_remainder = 1'b0;

// ---- Multicycle operations submodules ----

// Unsigned Multiplier
reg [31:0] multu_a = 32'd0;
reg [31:0] multu_b = 32'd0;
wire [63:0] multu_y;
reg multu_start = 1'b0;
wire multu_done;
Multu multu (
    .clk(clk),
    .reset(reset),
    .a(multu_a),
    .b(multu_b),
    .y(multu_y),
    .start(multu_start),
    .done(multu_done)
);

// Signed Multiplier
reg [31:0] mults_a = 32'd0;
reg [31:0] mults_b = 32'd0;
wire [63:0] mults_y;
reg mults_start = 1'b0;
wire mults_done;
Mults mults (
    .clk(clk),
    .reset(reset),
    .a(mults_a),
    .b(mults_b),
    .y(mults_y),
    .start(mults_start),
    .done(mults_done)
);

// Integer Divider (handles both signed/unsigned division and modulo)
reg [31:0] idiv_a = 32'd0;
reg [31:0] idiv_b = 32'd0;
reg idiv_signed = 1'b0;
reg idiv_start = 1'b0;
wire [31:0] idiv_quotient;
wire [31:0] idiv_remainder;
wire idiv_done;
IDivider idiv (
    .clk(clk),
    .reset(reset),
    .a(idiv_a),
    .b(idiv_b),
    .is_signed(idiv_signed),
    .start(idiv_start),
    .y_quotient(idiv_quotient),
    .y_remainder(idiv_remainder),
    .done(idiv_done)
);

// Fixed-point Divider
reg [31:0] fpdiv_a = 32'd0;
reg [31:0] fpdiv_b = 32'd0;
reg fpdiv_start = 1'b0;
wire [31:0] fpdiv_y;
wire fpdiv_done;
FPDivider fpdiv (
    .clk(clk),
    .reset(reset),
    .a(fpdiv_a),
    .b(fpdiv_b),
    .start(fpdiv_start),
    .y(fpdiv_y),
    .done(fpdiv_done)
);

// 64-bit Signed Multiplier (FP64 coprocessor)
reg mults64_start = 1'b0;
wire signed [127:0] mults64_y;
wire mults64_done;
Mults64 mults64 (
    .clk(clk),
    .reset(reset),
    .a(fp_a),
    .b(fp_b),
    .start(mults64_start),
    .y(mults64_y),
    .done(mults64_done)
);

always @(posedge clk)
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
        idiv_start <= 1'b0;
        idiv_a <= 32'd0;
        idiv_b <= 32'd0;
        idiv_signed <= 1'b0;
        idiv_want_remainder <= 1'b0;
        fpdiv_start <= 1'b0;
        fpdiv_a <= 32'd0;
        fpdiv_b <= 32'd0;
        mults64_start <= 1'b0;
        fp_result <= 64'd0;
    end
    else
    begin
        // Default assignments
        done <= 1'b0;
        y <= y;
        multu_start <= 1'b0;
        mults_start <= 1'b0;
        idiv_start <= 1'b0;
        fpdiv_start <= 1'b0;
        mults64_start <= 1'b0;

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

                    if (opcode == OP_DIVS || opcode == OP_DIVU ||
                        opcode == OP_MODS || opcode == OP_MODU)
                    begin
                        idiv_a <= a;
                        idiv_b <= b;
                        idiv_signed <= (opcode == OP_DIVS || opcode == OP_MODS);
                        idiv_want_remainder <= (opcode == OP_MODS || opcode == OP_MODU);
                        idiv_start <= 1'b1;
                        state <= STATE_WAIT_IDIV;
                    end

                    if (opcode == OP_DIVFP)
                    begin
                        fpdiv_a <= a;
                        fpdiv_b <= b;
                        fpdiv_start <= 1'b1;
                        state <= STATE_WAIT_FPDIV;
                    end

                    if (opcode == OP_MULSHI)
                    begin
                        mults_a <= a;
                        mults_b <= b;
                        mults_start <= 1'b1;
                        state <= STATE_WAIT_MULSHI;
                    end

                    if (opcode == OP_MULTUHI)
                    begin
                        multu_a <= a;
                        multu_b <= b;
                        multu_start <= 1'b1;
                        state <= STATE_WAIT_MULTUHI;
                    end

                    if (opcode == OP_FMUL)
                    begin
                        // fp_a and fp_b are wired directly from FP register file
                        mults64_start <= 1'b1;
                        state <= STATE_WAIT_MULTS64;
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

            STATE_WAIT_IDIV:
            begin
                if (idiv_done)
                begin
                    if (idiv_want_remainder)
                        y <= idiv_remainder;
                    else
                        y <= idiv_quotient;
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_FPDIV:
            begin
                if (fpdiv_done)
                begin
                    y <= fpdiv_y;
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_MULSHI:
            begin
                if (mults_done)
                begin
                    y <= mults_y[63:32]; // Return high 32 bits of signed multiply
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_MULTUHI:
            begin
                if (multu_done)
                begin
                    y <= multu_y[63:32]; // Return high 32 bits of unsigned multiply
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_MULTS64:
            begin
                if (mults64_done)
                begin
                    // 32.32 × 32.32 = 64.64, we want the middle 64 bits [95:32]
                    fp_result <= mults64_y[95:32];
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
