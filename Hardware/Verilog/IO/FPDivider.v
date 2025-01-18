/*
* 32-bit multicycle fixed-point divider
*/

module FPDivider #(
    parameter WIDTH=32,  // width of numbers in bits (integer and fractional)
    parameter FBITS=16   // fractional bits within WIDTH
    ) (
    input clk,    // clock
    input rst,    // reset
    input start,  // start calculation
    input write_a,
    output reg busy = 1'b0,   // calculation in progress
    output  reg    done = 1'b0,   // calculation is complete (high for one tick)
    output   reg   valid = 1'b0,  // result is valid
    output   reg   dbz = 1'b0,    // divide by zero
    output   reg   ovf = 1'b0,    // overflow
    input signed [WIDTH-1:0] a_in,   // dividend (numerator)
    input signed [WIDTH-1:0] b,   // divisor (denominator)
    output reg signed [WIDTH-1:0] val = 32'd0 // result value: quotient
    );


    reg signed [WIDTH-1:0] a = 0;

    always @(posedge clk)
    begin
        if (write_a)
        begin
            a <= a_in;
        end
        
    end

    localparam WIDTHU = WIDTH - 1;                 // unsigned widths are 1 bit narrower
    localparam FBITSW = (FBITS == 0) ? 1 : FBITS;  // avoid negative vector width when FBITS=0
    localparam SMALLEST = {1'b1, {WIDTHU{1'b0}}};  // smallest negative number

    localparam ITER = WIDTHU + FBITS;  // iteration count: unsigned input width + fractional bits
    reg [$clog2(ITER):0] i;          // iteration counter (allow ITER+1 iterations for rounding)

    reg a_sig = 1'b0;
    reg b_sig = 1'b0;
    reg sig_diff = 1'b0;      // signs of inputs and whether different

    reg [WIDTHU-1:0] au = 0;
    reg [WIDTHU-1:0] bu = 0;         // absolute version of inputs (unsigned)

    reg [WIDTHU-1:0] quo = 0;
    reg [WIDTHU-1:0] quo_next = 0;  // intermediate quotients (unsigned)

    reg [WIDTHU:0] acc = 0;
    reg [WIDTHU:0] acc_next = 0;    // accumulator (unsigned but 1 bit wider)

    // input signs
    always @(*)
    begin
        a_sig = a[WIDTH-1+:1];
        b_sig = b[WIDTH-1+:1];
    end

    // division algorithm iteration
    always @(*)
    begin
        if (acc >= {1'b0, bu}) begin
            acc_next = acc - bu;
            {acc_next, quo_next} = {acc_next[WIDTHU-1:0], quo, 1'b1};
        end else begin
            {acc_next, quo_next} = {acc, quo} << 1;
        end
    end

    // calculation state machine
    parameter IDLE    = 5'd0;
    parameter INIT    = 5'd1;
    parameter CALC    = 5'd2;
    parameter ROUND   = 5'd3;
    parameter SIGN    = 5'd4;
    parameter DONE    = 5'd5;

    reg [2:0] state = IDLE;

    always @(posedge clk) begin
        done <= 0;
        case (state)
            INIT: begin
                state <= CALC;
                ovf <= 0;
                i <= 0;
                {acc, quo} <= {{WIDTHU{1'b0}}, au, 1'b0};  // initialize calculation
            end
            CALC: begin
                if (i == WIDTHU-1 && quo_next[WIDTHU-1:WIDTHU-FBITSW] != 0) begin  // overflow
                    state <= DONE;
                    busy <= 0;
                    done <= 1;
                    ovf <= 1;
                end else begin
                    if (i == ITER-1) state <= ROUND;  // calculation complete after next iteration
                    i <= i + 1;
                    acc <= acc_next;
                    quo <= quo_next;
                end
            end
            ROUND: begin  // Gaussian rounding
                state <= SIGN;
                if (quo_next[0] == 1'b1) begin  // next digit is 1, so consider rounding
                    // round up if quotient is odd or remainder is non-zero
                    if (quo[0] == 1'b1 || acc_next[WIDTHU:1] != 0) quo <= quo + 1;
                end
            end
            SIGN: begin  // adjust quotient sign if non-zero and input signs differ
                state <= DONE;
                if (quo != 0) val <= (sig_diff) ? {1'b1, -quo} : {1'b0, quo};
                busy <= 0;
                done <= 1;
                valid <= 1;
            end
            DONE: begin
                done <= 1;
                state <= IDLE;
            end
            default: begin  // IDLE
                if (start) begin
                    valid <= 0;
                    if (b == 0) begin  // divide by zero
                        state <= DONE;
                        busy <= 0;
                        done <= 1;
                        dbz <= 1;
                        ovf <= 0;
                    end else if (a == SMALLEST || b == SMALLEST) begin  // overflow
                        state <= DONE;
                        busy <= 0;
                        done <= 1;
                        dbz <= 0;
                        ovf <= 1;
                    end else begin
                        state <= INIT;
                        au <= (a_sig) ? -a[WIDTHU-1:0] : a[WIDTHU-1:0];  // register abs(a)
                        bu <= (b_sig) ? -b[WIDTHU-1:0] : b[WIDTHU-1:0];  // register abs(b)
                        sig_diff <= (a_sig ^ b_sig);  // register input sign difference
                        busy <= 1;
                        dbz <= 0;
                        ovf <= 0;
                    end
                end
            end
        endcase
        if (rst) begin
            state <= IDLE;
            busy <= 0;
            done <= 0;
            valid <= 0;
            dbz <= 0;
            ovf <= 0;
            val <= 0;
        end
    end
endmodule
