// Board LEDs and DIP switches
module board_io (
    input  wire       clk,
    input  wire       reset,

    input  wire       set_leds,     // takes leds_in
    input  wire [4:0] leds_in,
    output reg  [4:0] leds,         // 1 is on

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
        leds <= 5'd0;
    else if (set_leds)
        leds <= leds_in;
end

endmodule
