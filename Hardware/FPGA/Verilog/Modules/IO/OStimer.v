/*
 * OStimer
 * One shot timer that counts in milliseconds
 * Uses a delay of 99999 cycles per timer_value (1ms at 100MHz)
 */
module OStimer (
    input wire          clk,
    input wire          reset,

    input wire  [31:0]  timer_value,
    input wire          trigger,
    input wire          set_value,

    output reg          interrupt = 1'b0
);

// ---- State Machine ----
localparam
    STATE_IDLE  = 0,
    STATE_START = 1,
    STATE_DONE  = 2;

parameter DELAY = 99999; // Clock cycles delay per timer_value

reg [31:0] counter_value = 32'd0;
reg [31:0] delay_counter = 32'd0;
reg [1:0]  state = STATE_IDLE;

always @(posedge clk)
begin
    if (reset)
    begin
        counter_value <= 32'd0;
        delay_counter <= 32'd0;
        state         <= STATE_IDLE;
        interrupt     <= 1'b0;
    end
    else
    begin
        if (set_value)
        begin
            counter_value <= timer_value;
        end
        else
        begin
            case (state)
                STATE_IDLE:
                begin
                    if (trigger)
                    begin
                        state <= STATE_START;
                        delay_counter <= DELAY;
                    end
                end

                STATE_START:
                begin
                    if (counter_value == 32'd0)
                    begin
                        state <= STATE_DONE;
                        interrupt <= 1'b1;
                    end
                    else
                    begin
                        if (delay_counter == 32'd0)
                        begin
                            counter_value <= counter_value - 1'b1;
                            delay_counter <= DELAY;
                        end
                        else
                        begin
                            delay_counter <= delay_counter - 1'b1;
                        end
                    end
                end

                STATE_DONE:
                begin
                    interrupt <= 1'b0;
                    state <= STATE_IDLE;
                end
            endcase
        end
    end
end

endmodule
