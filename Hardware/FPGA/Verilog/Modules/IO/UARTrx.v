/*
 * UARTrx
 * UART Receiver module
 * Receives 8 bits of serial data with one start bit, one stop bit, and no parity bit
 * When receive is complete, done will be driven high for one clock cycle
 *
 * Set Parameter CLKS_PER_BIT as follows:
 * CLKS_PER_BIT = (Frequency of clk) / (Frequency of UART)
 * Example: 50 MHz Clock, 1 MBaud UART: (50000000) / (1000000) = 50
 */
module UARTrx #(
    parameter CLKS_PER_BIT = 50     // 1 MBaud @ 50 MHz
) (
    //========================
    // System interface
    //========================
    input  wire         clk,
    input  wire         reset,

    //========================
    // Control interface
    //========================
    output reg          done = 1'b0,    // Byte received (high for one cycle)
    output reg  [7:0]   data = 8'd0,    // Received byte

    //========================
    // UART interface
    //========================
    input  wire         rx              // Serial input
);

//========================
// State Machine
//========================
localparam
    STATE_IDLE      = 3'd0,
    STATE_START_BIT = 3'd1,
    STATE_DATA_BITS = 3'd2,
    STATE_STOP_BIT  = 3'd3,
    STATE_DONE      = 3'd4;

reg [2:0] state = STATE_IDLE;

//========================
// Internal Registers
//========================
reg [8:0] clk_count = 9'd0;     // Bit period counter
reg [2:0] bit_index = 3'd0;     // Current bit being received (0-7)
reg [7:0] rx_data = 8'd0;       // Shift register for received data

// Double-register for metastability protection
reg rx_sync1 = 1'b1;
reg rx_sync2 = 1'b1;

// Synchronize incoming RX signal
always @(posedge clk)
begin
    if (reset)
    begin
        rx_sync1 <= 1'b1;
        rx_sync2 <= 1'b1;
    end
    else
    begin
        rx_sync1 <= rx;
        rx_sync2 <= rx_sync1;
    end
end

always @(posedge clk)
begin
    if (reset)
    begin
        state <= STATE_IDLE;
        clk_count <= 9'd0;
        bit_index <= 3'd0;
        rx_data <= 8'd0;
        data <= 8'd0;
        done <= 1'b0;
    end
    else
    begin
        // Default assignment
        done <= 1'b0;

        case (state)
            STATE_IDLE:
            begin
                clk_count <= 9'd0;
                bit_index <= 3'd0;

                // Detect start bit (falling edge to low)
                if (rx_sync2 == 1'b0)
                begin
                    state <= STATE_START_BIT;
                end
            end

            STATE_START_BIT:
            begin
                // Sample at middle of start bit to verify it's still low
                if (clk_count == (CLKS_PER_BIT - 1) / 2)
                begin
                    if (rx_sync2 == 1'b0)
                    begin
                        // Valid start bit, proceed to data bits
                        clk_count <= 9'd0;
                        state <= STATE_DATA_BITS;
                    end
                    else
                    begin
                        // False start, return to idle
                        state <= STATE_IDLE;
                    end
                end
                else
                begin
                    clk_count <= clk_count + 1'b1;
                end
            end

            STATE_DATA_BITS:
            begin
                // Sample in middle of each data bit
                if (clk_count < CLKS_PER_BIT - 1)
                begin
                    clk_count <= clk_count + 1'b1;
                end
                else
                begin
                    clk_count <= 9'd0;
                    rx_data[bit_index] <= rx_sync2;     // LSB first

                    if (bit_index < 7)
                    begin
                        bit_index <= bit_index + 1'b1;
                    end
                    else
                    begin
                        bit_index <= 3'd0;
                        state <= STATE_STOP_BIT;
                    end
                end
            end

            STATE_STOP_BIT:
            begin
                // Wait for stop bit period
                if (clk_count < CLKS_PER_BIT - 1)
                begin
                    clk_count <= clk_count + 1'b1;
                end
                else
                begin
                    clk_count <= 9'd0;
                    state <= STATE_DONE;
                end
            end

            STATE_DONE:
            begin
                data <= rx_data;
                done <= 1'b1;
                state <= STATE_IDLE;
                $display("%d: uart_rx byte 0x%h", $time, rx_data);
            end

            default:
            begin
                state <= STATE_IDLE;
            end
        endcase
    end
end

endmodule
