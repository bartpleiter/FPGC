module ActivityLED #(
    parameter integer HOLD_CYCLES = 24'd8000000
) (
    input  wire clk,
    input  wire reset,
    input  wire activity,
    output wire led
);

reg [23:0] hold_counter = 24'd0;

always @(posedge clk)
begin
    if (reset)
    begin
        hold_counter <= 24'd0;
    end
    else if (activity)
    begin
        hold_counter <= HOLD_CYCLES - 1;
    end
    else if (hold_counter != 24'd0)
    begin
        hold_counter <= hold_counter - 24'd1;
    end
end

assign led = (hold_counter != 24'd0);

endmodule
