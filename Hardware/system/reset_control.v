// Reset
// From the PLL, the reset button, and the Host: RTS# low with DTR# high for 1 ms.
module reset_control #(
    parameter PC_RESET_CYCLES = 100000  // 1 ms
) (
    input  wire clk,
    input  wire locked,    // the PLL's clock is stable
    input  wire button_n,  // low while pressed
    input  wire rts_n,     // from the USB serial chip
    input  wire dtr_n,
    output reg  reset = 1'b1
);

localparam COUNT_BITS = $clog2(PC_RESET_CYCLES + 1);

// Synchronizers, 0 after configuration, which is a reset
reg [3:0] pins_sync = 4'd0;
reg [3:0] pins      = 4'd0;

always @(posedge clk) begin
    pins_sync <= {locked, button_n, rts_n, dtr_n};
    pins      <= pins_sync;
end

wire stable  = pins[3];
wire pressed = !pins[2];
wire pc      = !pins[1] && pins[0];

// How long the Host's combination has lasted
reg [COUNT_BITS-1:0] pc_count = {COUNT_BITS{1'b0}};

wire pc_reset = pc_count == PC_RESET_CYCLES[COUNT_BITS-1:0];

always @(posedge clk) begin
    if (!pc)
        pc_count <= {COUNT_BITS{1'b0}};
    else if (!pc_reset)
        pc_count <= pc_count + 1'b1;
end

always @(posedge clk)
    reset <= !stable || pressed || pc_reset;

endmodule
