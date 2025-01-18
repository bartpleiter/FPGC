// Visualizes activity using a LED
// Works by setting a minimum amount of cycles that the LED has to stay on before turning off

module LEDvisualizer
#(parameter MIN_CLK = 10000)
(
    input clk,
    input reset,
    input activity,
    output LED
);

localparam s_idle       = 0;
localparam s_busy       = 1;

reg [31:0] counterValue = 32'd0;    // 32 bits for timerValue
reg [1:0] state = s_idle;           // state of module

assign LED = !(state == s_busy);    // inverted because led is active low

always @(posedge clk)
begin
    if (reset)
    begin
        counterValue                <= 32'd0;
        state                       <= s_idle;
    end
    else
    begin
        case (state)
            s_idle:
            begin
                if (activity)
                begin
                    counterValue <= MIN_CLK;
                    state <= s_busy;
                end
            end
            s_busy:
            begin
                if (activity)
                begin
                    counterValue <= MIN_CLK;
                end
                else 
                begin
                    if (counterValue == 32'd0)
                    begin
                        state <= s_idle;
                    end
                    else
                    begin
                        counterValue <= counterValue - 1'b1;
                    end
                end
            end
        endcase
    end
end

endmodule
