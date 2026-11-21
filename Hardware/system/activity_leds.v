// Activity LEDs
// After a tick period with activity, an LED is on for one period, then off for one.
module activity_leds #(
    parameter LEDS      = 2,
    parameter TICK_BITS = 23  // a tick every 2^23 cycles: 84 ms
) (
    input  wire            clk,
    input  wire            reset,
    input  wire [LEDS-1:0] activity,  // a level, from any clock
    output reg  [LEDS-1:0] leds       // 1 is on
);

reg [LEDS-1:0]      activity_sync;
reg [LEDS-1:0]      active;
reg [LEDS-1:0]      seen;     // activity in this period
reg [TICK_BITS-1:0] counter;

wire tick = &counter;

always @(posedge clk) begin
    activity_sync <= activity;
    active        <= activity_sync;
end

always @(posedge clk) begin
    if (reset) begin
        counter <= {TICK_BITS{1'b0}};
        seen    <= {LEDS{1'b0}};
        leds    <= {LEDS{1'b0}};
    end else begin
        counter <= counter + 1'b1;
        if (tick) begin
            leds <= seen & ~leds;
            seen <= active;
        end else begin
            seen <= seen | active;
        end
    end
end

endmodule
