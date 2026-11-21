// UART transmitter
module uart_tx #(
    parameter CLOCKS_PER_BIT = 100
) (
    input  wire       clk,
    input  wire       reset,

    input  wire       start,  // high for one cycle
    input  wire [7:0] data,
    output wire       busy,

    output reg        tx = 1'b1  // high from configuration on
);

localparam [15:0] BIT = CLOCKS_PER_BIT;

reg [8:0]  shift;
reg [3:0]  bits;
reg [15:0] clocks;

assign busy = bits != 4'd0;

always @(posedge clk) begin
    if (reset) begin
        tx     <= 1'b1;
        shift  <= 9'h1FF;
        bits   <= 4'd10; // To make sure the line stays high for a while after a reset
        clocks <= 16'd0;
    end else if (!busy) begin
        if (start) begin
            tx    <= 1'b0;
            shift <= {1'b1, data};
            bits  <= 4'd10;
        end
    end else if (clocks == BIT - 16'd1) begin
        tx     <= shift[0];
        shift  <= {1'b1, shift[8:1]};
        bits   <= bits - 4'd1;
        clocks <= 16'd0;
    end else begin
        clocks <= clocks + 16'd1;
    end
end

endmodule
