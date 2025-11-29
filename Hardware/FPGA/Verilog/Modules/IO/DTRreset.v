/*
* Generates a reset pulse for 10 cycles on falling edge of DTR.
*/
module DtrReset (
  input wire  clk,
  input wire  dtr,
  output reg  reset_dtr = 1'b0
);

reg state = 1'b0;

localparam
    STATE_IDLE  = 1'b0,
    STATE_PULSE = 1'b1;


reg dtr_prev = 1'b1;            // Previous dtr value to check for falling edge
reg [3:0] pulse_counter = 4'd0; // Counter for keeping reset high

always @(posedge clk)
begin
    dtr_prev <= dtr;

    case (state)
        STATE_IDLE:
        begin
            if (dtr_prev && !dtr) // Falling edge
            begin
                state <= STATE_PULSE;
                pulse_counter <= 4'b1111;
                reset_dtr <= 1'b1;
            end
        end
        STATE_PULSE:
        begin
            pulse_counter <= pulse_counter - 1'b1;
            if (pulse_counter == 4'd0)
            begin
                state <= STATE_IDLE;
                pulse_counter <= 4'd0;
                reset_dtr <= 1'b0;
            end
        end
    endcase
end

endmodule
