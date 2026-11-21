// UART
// Serial port with 32-byte buffer both ways.
module uart #(
    parameter CLOCKS_PER_BIT = 100
) (
    input  wire       clk,
    input  wire       reset,

    // Transmit
    input  wire       transmit,       // puts tx_byte in the transmit buffer
    input  wire [7:0] tx_byte,
    output wire       tx_full,

    // Receive
    input  wire       receive,        // takes the oldest byte
    output reg        received,       // rx_byte is the byte taken
    output wire [7:0] rx_byte,
    output wire       rx_waiting,     // a byte is in the receive buffer

    // Errors
    input  wire       clear,          // clears both errors
    output reg        overflow,       // a byte was lost, the buffer was full
    output reg        framing_error,  // a byte without stop bit was dropped

    // Interrupt
    input  wire       configure,      // takes irq_enable_in
    input  wire       irq_enable_in,
    output reg        irq_enable,
    output wire       irq,

    // Serial line
    input  wire       rx,
    output wire       tx
);

reg        tx_starting;
wire       tx_empty;
wire       tx_busy;
wire [7:0] tx_data;
wire       rx_empty;
wire       rx_full;
wire       rx_valid;
wire [7:0] rx_data;
wire       rx_framing_error;

wire tx_pop = !tx_busy && !tx_starting && !tx_empty;

fifo #(
    .WIDTH(8),
    .DEPTH_BITS(5)
) tx_buffer (
    .clk(clk),
    .reset(reset),
    .push(transmit),
    .write_data(tx_byte),
    .pop(tx_pop),
    .read_data(tx_data),
    .empty(tx_empty),
    .full(tx_full)
);

uart_tx #(
    .CLOCKS_PER_BIT(CLOCKS_PER_BIT)
) transmitter (
    .clk(clk),
    .reset(reset),
    .start(tx_starting),
    .data(tx_data),
    .busy(tx_busy),
    .tx(tx)
);

uart_rx #(
    .CLOCKS_PER_BIT(CLOCKS_PER_BIT)
) receiver (
    .clk(clk),
    .reset(reset),
    .rx(rx),
    .valid(rx_valid),
    .data(rx_data),
    .framing_error(rx_framing_error)
);

fifo #(
    .WIDTH(8),
    .DEPTH_BITS(5)
) rx_buffer (
    .clk(clk),
    .reset(reset),
    .push(rx_valid),
    .write_data(rx_data),
    .pop(receive),
    .read_data(rx_byte),
    .empty(rx_empty),
    .full(rx_full)
);

always @(posedge clk) begin
    if (reset) begin
        tx_starting   <= 1'b0;
        received      <= 1'b0;
        irq_enable    <= 1'b0;
        overflow      <= 1'b0;
        framing_error <= 1'b0;
    end else begin
        tx_starting <= tx_pop;
        received    <= receive && !rx_empty;
        if (configure)
            irq_enable <= irq_enable_in;
        overflow      <= (rx_valid && rx_full) || (overflow && !clear);
        framing_error <= rx_framing_error || (framing_error && !clear);
    end
end

assign rx_waiting = !rx_empty;
assign irq        = irq_enable && rx_waiting;

endmodule
