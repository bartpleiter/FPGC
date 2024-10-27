/*
* Communicates with SNES (and probably also NES) controller
* Based on https://opencores.org/projects/nescontroller by Yvo Zoer
*  with modifications to make it work for a single SNES controller
*  and to make the interface work nicer with the MU
*/

module NESpadReader (
    input clk,
    input reset,
    input frame, //start
    output reg nesl = 1'b0,
    output reg nesc = 1'b0,
    input nesd,
    output reg [15:0] nesState = 16'd0,
    output done
    );

    
 
    // states for state-machine
    parameter STATE_IDLE        = 3'd0;
    parameter STATE_LATCH_HI    = 3'd1; 
    parameter STATE_LATCH_LO    = 3'd2;
    parameter STATE_CLOCK_LO    = 3'd3;
    parameter STATE_CLOCK_HI    = 3'd4;
    parameter STATE_DONE        = 3'd5;

    assign done = (state == STATE_DONE);
 
    // start bit to catch single clock frame event
    reg start = 1'b0;
    always @(posedge clk)
        if (reset)
            start <= 1'b0;
        else if ( frame )
            start <= 1'b1;
        else if ( state != STATE_IDLE )
            start <= 1'b0;
 
    // clock divider / enable -- clock needs to be around 1.5mhz or lower
    reg [5:0] clkdiv = 6'd0;
    always @(posedge clk)
        if (reset)
            clkdiv <= 6'd0;
        else
            clkdiv <= clkdiv + 1'd1;
 
    wire enable = &clkdiv;
 
    // main state machine
    reg [2:0] state = 3'd0;
    reg [2:0] next_state = 3'd0;
    always @(*)
        case (state)
            STATE_IDLE : begin
                if ( start )
                    next_state = STATE_LATCH_HI;
                else
                    next_state = STATE_IDLE;
                end
            STATE_LATCH_HI : next_state = STATE_LATCH_LO;
            STATE_LATCH_LO : next_state = STATE_CLOCK_LO;
            STATE_CLOCK_LO : next_state = STATE_CLOCK_HI;
            STATE_CLOCK_HI : begin
                if ( &count )
                begin
                    next_state = STATE_DONE;
                end
                else
                    next_state = STATE_CLOCK_LO;
                end
            STATE_DONE : begin
                next_state = STATE_IDLE;
                end
            default : begin
                next_state = STATE_IDLE;
                end
        endcase
 

    always @(posedge clk)
        if (reset)
            nesl <= 1'b0;
        else
            nesl <= (state == STATE_LATCH_HI);
 
    always @(posedge clk)
        if (reset)
            nesc <= 1'b1;
        else
            nesc = (state == STATE_CLOCK_LO ) ? 1'b0 : 1'b1;
 
    reg [3:0] count = 4'd0;
    reg [3:0] next_count = 4'd0;
    always @(*)
        if ( state == STATE_CLOCK_HI )
            next_count = count + 4'd1;
        else
            next_count = count;
 
    reg [15:0] next_nesState = 16'd0;
    always @(*)
        if ( state == STATE_CLOCK_LO )
            begin
                next_nesState = { nesState[14:0], nesd };
            end
        else
            begin
                next_nesState = nesState;
            end
 
    always @(posedge clk)
        if (reset)
            begin
                state <= STATE_IDLE;
                count <= 4'd0;
                nesState <= 16'd0;
            end
        else if ( enable )
            begin
                state <= next_state;
                count <= next_count;
                nesState <= next_nesState;
            end
 
endmodule