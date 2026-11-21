// Board user LED and DIP switches
module board_io (
    input  wire       clk,
    input  wire       reset,

    input  wire       set_led,      // takes led_in
    input  wire       led_in,
    output reg        led,          // the user LED, 1 is on

    input  wire [3:0] switch_pins,  // DIP switches
    output reg  [3:0] switches      // synchronized
);

reg [3:0] switches_sync;

always @(posedge clk) begin
    switches_sync <= switch_pins;
    switches      <= switches_sync;
end

always @(posedge clk) begin
    if (reset)
        led <= 1'b0;
    else if (set_led)
        led <= led_in;
end

endmodule
