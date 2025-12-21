/*
 * IDivider
 * 32-bit multicycle signed or unsigned integer divider
 * Uses restoring division algorithm with magnitude conversion
 *
 * Takes DATA_WIDTH+2 cycles to complete a division
 */
module IDivider #(
    parameter DATA_WIDTH = 32
) (
    //========================
    // System interface
    //========================
    input  wire                     clk,
    input  wire                     reset,

    //========================
    // Control interface
    //========================
    input  wire [DATA_WIDTH-1:0]    a,          // Dividend
    input  wire [DATA_WIDTH-1:0]    b,          // Divisor
    input  wire                     is_signed,  // 1 for signed, 0 for unsigned
    input  wire                     start,      // Start division

    output reg  [DATA_WIDTH-1:0]    y_quotient = 0,  // Quotient result
    output reg  [DATA_WIDTH-1:0]    y_remainder = 0, // Remainder result
    output reg                      done = 1'b0      // Division complete
);

//========================
// State Machine
//========================
localparam
    STATE_IDLE      = 2'd0,
    STATE_CALC      = 2'd1,
    STATE_SIGN      = 2'd2,
    STATE_DONE      = 2'd3;

reg [1:0] state = STATE_IDLE;

//========================
// Internal Registers
//========================
localparam COUNT_WIDTH = $clog2(DATA_WIDTH + 1);

reg [COUNT_WIDTH-1:0] count = 0;                    // Iteration counter
reg [DATA_WIDTH-1:0] quotient = 0;                  // Working quotient
reg [DATA_WIDTH-1:0] remainder = 0;                 // Working remainder
reg [DATA_WIDTH-1:0] divisor = 0;                   // Stored divisor (unsigned)
reg dividend_neg = 1'b0;                            // Original dividend was negative
reg divisor_neg = 1'b0;                             // Original divisor was negative

// Combinational: trial subtraction
wire [DATA_WIDTH:0] trial_sub = {remainder, quotient[DATA_WIDTH-1]} - {1'b0, divisor};
wire trial_ge = ~trial_sub[DATA_WIDTH]; // MSB is 0 means result >= 0

always @(posedge clk)
begin
    if (reset)
    begin
        state <= STATE_IDLE;
        count <= 0;
        quotient <= 0;
        remainder <= 0;
        divisor <= 0;
        dividend_neg <= 1'b0;
        divisor_neg <= 1'b0;
        y_quotient <= 0;
        y_remainder <= 0;
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
                    // Remember signs for signed division
                    dividend_neg <= is_signed & a[DATA_WIDTH-1];
                    divisor_neg <= is_signed & b[DATA_WIDTH-1];

                    // Handle divide by zero
                    if (b == 0)
                    begin
                        y_quotient <= {DATA_WIDTH{1'b1}};       // -1 in two's complement
                        y_remainder <= a;
                        done <= 1'b1;
                        state <= STATE_IDLE;
                    end
                    else
                    begin
                        // Convert to positive magnitudes for calculation
                        // Use unsigned division then fix signs at end
                        if (is_signed && a[DATA_WIDTH-1])
                            quotient <= -a;     // abs(dividend) stored in quotient shift reg
                        else
                            quotient <= a;

                        if (is_signed && b[DATA_WIDTH-1])
                            divisor <= -b;      // abs(divisor)
                        else
                            divisor <= b;

                        remainder <= 0;
                        count <= 0;
                        state <= STATE_CALC;
                    end
                end
            end

            STATE_CALC:
            begin
                // Restoring division: shift and subtract
                if (trial_ge)
                begin
                    // Subtraction succeeded
                    remainder <= trial_sub[DATA_WIDTH-1:0];
                    quotient <= {quotient[DATA_WIDTH-2:0], 1'b1};
                end
                else
                begin
                    // Subtraction would underflow, restore (just shift)
                    remainder <= {remainder[DATA_WIDTH-2:0], quotient[DATA_WIDTH-1]};
                    quotient <= {quotient[DATA_WIDTH-2:0], 1'b0};
                end
                
                count <= count + 1'b1;
                
                if (count == DATA_WIDTH - 1)
                begin
                    state <= STATE_SIGN;
                end
            end

            STATE_SIGN:
            begin
                // Apply sign corrections for signed division
                // Quotient sign: negative if exactly one operand was negative
                // Remainder sign: same sign as dividend
                if (dividend_neg ^ divisor_neg)
                    y_quotient <= -quotient;
                else
                    y_quotient <= quotient;
                
                if (dividend_neg)
                    y_remainder <= -remainder;
                else
                    y_remainder <= remainder;
                
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
