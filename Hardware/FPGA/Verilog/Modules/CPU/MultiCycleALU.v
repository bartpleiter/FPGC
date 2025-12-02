/*
 * MultiCycleALU
 * Performs ARITHM instructions that require multiple cycles
 * Uses a state machine to orchestrate the operations to the relevant submodules based on opcode
 * While this does add latency, it simplifies the design and makes it behave similar to the Memory Unit
 * TODO: the divider modules should be reimplemented as they are now just copied from the memory mapped
 * FPGC6 design
 */
module MultiCycleALU (
    input wire          clk,
    input wire          reset,

    input wire          start,
    output reg          done = 1'b0,

    input wire  [31:0]  a,
    input wire  [31:0]  b,
    input wire  [3:0]   opcode,
    output reg  [31:0]  y = 32'd0
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
    STATE_WRITE_IDIV = 4'd4,
    STATE_WAIT_IDIV = 4'd5,
    STATE_WRITE_FPDIV = 4'd6,
    STATE_WAIT_FPDIV = 4'd7,
    STATE_DONE = 4'd15;

reg [3:0] state = STATE_IDLE;

// Track whether we want quotient or remainder for integer division
reg idiv_want_remainder = 1'b0;

// Multicycle operations submodules
// ================================

// Unsigned Multiplier
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

// Signed Multiplier
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

// Integer Divider (handles both signed/unsigned division and modulo)
reg [31:0] idiv_a = 32'd0;
reg [31:0] idiv_b = 32'd0;
reg idiv_signed = 1'b0;
reg idiv_write_a = 1'b0;
reg idiv_start = 1'b0;
wire [31:0] idiv_quotient;
wire [31:0] idiv_remainder;
wire idiv_ready;
IDivider idiv (
    .clk(clk),
    .rst(reset),
    .a(idiv_a),
    .b(idiv_b),
    .signed_ope(idiv_signed),
    .write_a(idiv_write_a),
    .start(idiv_start),
    .flush(1'b0),
    .quotient(idiv_quotient),
    .remainder(idiv_remainder),
    .ready(idiv_ready)
);

// Fixed-point Divider
reg [31:0] fpdiv_a = 32'd0;
reg [31:0] fpdiv_b = 32'd0;
reg fpdiv_write_a = 1'b0;
reg fpdiv_start = 1'b0;
wire fpdiv_busy;
wire fpdiv_done;
wire fpdiv_valid;
wire [31:0] fpdiv_val;
FPDivider fpdiv (
    .clk(clk),
    .rst(reset),
    .a_in(fpdiv_a),
    .b(fpdiv_b),
    .write_a(fpdiv_write_a),
    .start(fpdiv_start),
    .busy(fpdiv_busy),
    .done(fpdiv_done),
    .valid(fpdiv_valid),
    .dbz(),
    .ovf(),
    .val(fpdiv_val)
);

// Track previous ready state to detect completion
reg idiv_ready_prev = 1'b1;

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
        idiv_start <= 1'b0;
        idiv_write_a <= 1'b0;
        idiv_a <= 32'd0;
        idiv_b <= 32'd0;
        idiv_signed <= 1'b0;
        idiv_want_remainder <= 1'b0;
        idiv_ready_prev <= 1'b1;
        fpdiv_start <= 1'b0;
        fpdiv_write_a <= 1'b0;
        fpdiv_a <= 32'd0;
        fpdiv_b <= 32'd0;
    end
    else
    begin
        // Default assignments
        done <= 1'b0;
        y <= y;
        multu_start <= 1'b0;
        mults_start <= 1'b0;
        idiv_start <= 1'b0;
        idiv_write_a <= 1'b0;
        fpdiv_start <= 1'b0;
        fpdiv_write_a <= 1'b0;
        
        // Track previous idiv_ready state
        idiv_ready_prev <= idiv_ready;
        
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
                        idiv_write_a <= 1'b1;
                        // Don't start yet - wait for write_a to be processed
                        state <= STATE_WRITE_IDIV;
                    end

                    if (opcode == OP_DIVFP)
                    begin
                        fpdiv_a <= a;
                        fpdiv_b <= b;
                        fpdiv_write_a <= 1'b1;
                        // Don't start yet - wait for write_a to be processed
                        state <= STATE_WRITE_FPDIV;
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

            STATE_WRITE_IDIV:
            begin
                // Dividend has been written, now start the division
                idiv_start <= 1'b1;
                state <= STATE_WAIT_IDIV;
            end

            STATE_WAIT_IDIV:
            begin
                // IDivider: ready goes low when busy, back to high when done
                // Detect rising edge of ready (was low, now high)
                if (idiv_ready && !idiv_ready_prev)
                begin
                    if (idiv_want_remainder)
                        y <= idiv_remainder;
                    else
                        y <= idiv_quotient;
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WRITE_FPDIV:
            begin
                // Dividend has been written, now start the FP division
                fpdiv_start <= 1'b1;
                state <= STATE_WAIT_FPDIV;
            end

            STATE_WAIT_FPDIV:
            begin
                if (fpdiv_done && fpdiv_valid)
                begin
                    y <= fpdiv_val;
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
