/*
 * FPDivider
 * 32-bit multicycle fixed-point signed divider
 * Uses restoring division algorithm with Q16.16 fixed point format
 *
 * For fixed-point division: result = (a << FBITS) / b
 * This maintains the fractional precision in the quotient
 */
module FPDivider #(
    parameter WIDTH = 32,   // Width of numbers in bits
    parameter FBITS = 16    // Fractional bits within WIDTH
) (
    //========================
    // System interface
    //========================
    input  wire                     clk,
    input  wire                     reset,

    //========================
    // Control interface
    //========================
    input  wire signed [WIDTH-1:0]  a,          // Dividend (fixed-point)
    input  wire signed [WIDTH-1:0]  b,          // Divisor (fixed-point)
    input  wire                     start,      // Start division

    output reg signed [WIDTH-1:0]   y = 0,      // Quotient result (fixed-point)
    output reg                      done = 1'b0 // Division complete
);

//========================
// State Machine
//========================
localparam
    STATE_IDLE  = 2'd0,
    STATE_CALC  = 2'd1,
    STATE_SIGN  = 2'd2,
    STATE_DONE  = 2'd3;

reg [1:0] state = STATE_IDLE;

//========================
// Internal Parameters
//========================
localparam WIDTHU = WIDTH - 1;                              // Unsigned width (sign bit excluded)
localparam ITER = WIDTHU + FBITS;                           // Total iterations needed

//========================
// Internal Registers
//========================
reg [$clog2(ITER+1):0] count = 0;                           // Iteration counter
reg [WIDTHU-1:0] quotient = 0;                              // Working quotient (unsigned)
reg [WIDTHU:0] accumulator = 0;                             // Accumulator (1 bit wider for subtraction)
reg [WIDTHU-1:0] divisor_u = 0;                             // Unsigned divisor
reg sign_diff = 1'b0;                                       // Signs differ flag

// Combinational: trial subtraction
wire [WIDTHU:0] trial_sub = accumulator - {1'b0, divisor_u};
wire trial_ge = ~trial_sub[WIDTHU];                         // Result >= 0 if MSB is 0

// Helper wires for absolute value computation
wire [WIDTHU-1:0] a_abs = a[WIDTH-1] ? -a[WIDTHU-1:0] : a[WIDTHU-1:0];
wire [WIDTHU-1:0] b_abs = b[WIDTH-1] ? -b[WIDTHU-1:0] : b[WIDTHU-1:0];

always @(posedge clk)
begin
    if (reset)
    begin
        state <= STATE_IDLE;
        count <= 0;
        quotient <= 0;
        accumulator <= 0;
        divisor_u <= 0;
        sign_diff <= 1'b0;
        y <= 0;
        done <= 1'b0;
    end
    else
    begin
        // Default assignment
        done <= 1'b0;

        case (state)
            STATE_IDLE:
            begin
                if (start)
                begin
                    // Handle divide by zero
                    if (b == 0)
                    begin
                        y <= {WIDTH{1'b1}};     // Return -1 for div by zero
                        done <= 1'b1;
                        state <= STATE_IDLE;
                    end
                    else
                    begin
                        // Store sign difference
                        sign_diff <= a[WIDTH-1] ^ b[WIDTH-1];
                        
                        // Initialize for fixed-point division
                        // The quotient register is loaded with |a| initially, 
                        // accumulator starts at 0. As we iterate, bits shift from
                        // quotient into accumulator for the division.
                        accumulator <= {{WIDTHU{1'b0}}, a_abs[WIDTHU-1]};
                        quotient <= {a_abs[WIDTHU-2:0], 1'b0};
                        divisor_u <= b_abs;
                        
                        count <= 0;
                        state <= STATE_CALC;
                    end
                end
            end

            STATE_CALC:
            begin
                // Restoring division iteration
                if (trial_ge)
                begin
                    // Subtraction succeeded: use result and shift in 1
                    accumulator <= {trial_sub[WIDTHU-1:0], quotient[WIDTHU-1]};
                    quotient <= {quotient[WIDTHU-2:0], 1'b1};
                end
                else
                begin
                    // Subtraction would underflow: just shift in 0
                    accumulator <= {accumulator[WIDTHU-1:0], quotient[WIDTHU-1]};
                    quotient <= {quotient[WIDTHU-2:0], 1'b0};
                end
                
                count <= count + 1'b1;
                
                if (count == ITER - 1)
                begin
                    state <= STATE_SIGN;
                end
            end

            STATE_SIGN:
            begin
                // Apply sign to result
                if (quotient != 0 && sign_diff)
                    y <= {1'b1, -quotient};
                else
                    y <= {1'b0, quotient};
                
                state <= STATE_DONE;
            end

            STATE_DONE:
            begin
                done <= 1'b1;
                state <= STATE_IDLE;
            end

            default:
            begin
                state <= STATE_IDLE;
            end
        endcase
    end
end

endmodule
