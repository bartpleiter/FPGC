// One shot timer that counts in milliseconds
// Uses a delay of sysClkMHz-1 cycles per timerValue

module OStimer(
    input clk,
    input reset,
    input [31:0] timerValue,
    input trigger,
    input setValue,
    output reg interrupt = 1'b0
);

// States
localparam s_idle        = 0;
localparam s_start       = 1;
localparam s_done        = 2;

parameter delay         = 49999; // Clock cycles delay per timerValue, could eventually become programmable, should then default to 1ms

reg [31:0] counterValue = 32'd0;            // 32 bits for timerValue
reg [31:0] delayCounter = 32'd0;            // counter for timer delay
reg [1:0] state = s_idle;                     // state of timer

always @(posedge clk)
begin
    if (reset)
    begin
        counterValue                <= 32'd0;
        delayCounter                <= 32'd0;
        state                       <= s_idle;
        interrupt                   <= 1'd0;  
    end
    else
    begin
        if (setValue)
        begin
            counterValue <= timerValue;
        end
        else 
        begin
            
            case (state)
                s_idle:
                begin
                    if (trigger)
                    begin
                        state <= s_start;
                        delayCounter <= delay;
                    end
                    
                end
                s_start:
                begin
                    if (counterValue == 32'd0)
                    begin
                        state <= s_done;
                        interrupt <= 1'b1;
                    end
                    else 
                    begin

                        if (delayCounter == 32'd0)
                        begin
                            counterValue <= counterValue - 1'b1;
                            delayCounter <= delay;
                        end
                        else 
                        begin
                            delayCounter <= delayCounter - 1'b1;    
                        end
                    end
                end
                s_done:
                begin
                    interrupt <= 1'b0;
                    state <= s_idle;
                end

            endcase

        end

        
    end
end


endmodule


