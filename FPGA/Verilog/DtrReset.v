/*
* Generates reset pulse for 10 cycles on falling edge of DTR.
* Assumes incoming DTR signal is stabilized.
*/
module DtrReset (
  input     clk, dtr,
  output reg  dtrRst
);


reg [1:0] state;

parameter s_idle        = 0;
parameter s_pulse       = 1;


reg dtr_prev;               //previous dtr value to check for falling edge
reg [3:0] pulse_counter;    //counter for keeping reset high

//Make unstable signal stable at the base clock
always @(posedge clk)
begin
    case (state)
        s_idle:
        begin
            if (dtr_prev && !dtr) //falling edge
            begin
                state <= s_pulse;
                pulse_counter <= 4'b1111;
                dtrRst <= 1'b1;
            end
        end
        s_pulse:
        begin
            pulse_counter <= pulse_counter - 1'b1;
            if (pulse_counter == 4'd0)
            begin
                dtrRst <= 1'b0;
                state <= s_idle;
            end
        end
        default:
        begin
            state <= s_idle;
        end
    endcase
    dtr_prev <= dtr;
end
    
initial
begin
    dtrRst <= 1'b0;
    dtr_prev <= 1'b0;
    state <= s_idle;
    pulse_counter <= 4'd0;
end

endmodule