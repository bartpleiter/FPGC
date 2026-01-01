/*
 * UARTtx
 * UART Transmitter module
 * Transmits 8 bits of serial data with one start bit, one stop bit, and no parity bit
 * When transmit is complete, done will be driven high for one clock cycle
 *
 * Set Parameter CLKS_PER_BIT as follows:
 * CLKS_PER_BIT = (Frequency of clk) / (Frequency of UART)
 * Example: 100 MHz Clock, 1 MBaud UART: (100000000) / (1000000) = 100
 */
module UARTtx #(
    parameter CLKS_PER_BIT = 100,   // 1 MBaud @ 100 MHz
    parameter ENABLE_DISPLAY = 1
) (
    //========================
    // System interface
    //========================
    input  wire         clk,
    input  wire         reset,

    //========================
    // Control interface
    //========================
    input  wire         start,              // Start transmission (active high pulse)
    input  wire [7:0]   data,               // Byte to transmit
    output reg          done = 1'b0,        // Transmission complete (high for one cycle)
    output reg          tx_active = 1'b0,   // Transmission in progress

    //========================
    // UART interface
    //========================
    output reg          tx = 1'b1   // Serial output (idle high)
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
reg [2:0] bit_index = 3'd0;     // Current bit being transmitted (0-7)
reg [7:0] tx_data = 8'd0;       // Latched transmit data

always @(posedge clk)
begin
    if (reset)
    begin
        state <= STATE_IDLE;
        clk_count <= 9'd0;
        bit_index <= 3'd0;
        tx_data <= 8'd0;
        tx_active <= 1'b0;
        done <= 1'b0;
        tx <= 1'b1;
    end
    else
    begin
        // Default assignments
        done <= 1'b0;
        tx_active <= 1'b0;

        case (state)
            STATE_IDLE:
            begin
                tx <= 1'b1;             // Idle high
                clk_count <= 9'd0;
                bit_index <= 3'd0;

                if (start)
                begin
                    tx_data <= data;
                    tx_active <= 1'b1;
                    state <= STATE_START_BIT;
                    if (ENABLE_DISPLAY == 1)
                    begin
                        $display("%0t UART TX: %02h", $time, data);
                    end
                end
            end

            STATE_START_BIT:
            begin
                tx <= 1'b0;             // Start bit is low
                tx_active <= 1'b1;

                if (clk_count < CLKS_PER_BIT - 1)
                begin
                    clk_count <= clk_count + 1'b1;
                end
                else
                begin
                    clk_count <= 9'd0;
                    state <= STATE_DATA_BITS;
                end
            end

            STATE_DATA_BITS:
            begin
                tx <= tx_data[bit_index]; // Send LSB first
                tx_active <= 1'b1;

                if (clk_count < CLKS_PER_BIT - 1)
                begin
                    clk_count <= clk_count + 1'b1;
                end
                else
                begin
                    clk_count <= 9'd0;

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
                tx <= 1'b1;             // Stop bit is high
                tx_active <= 1'b1;
                
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
                done <= 1'b1;
                
                // Wait for start to go low before returning to idle
                // This prevents retriggering if start is held high
                if (!start)
                begin
                    state <= STATE_IDLE;
                end
            end

            default:
            begin
                state <= STATE_IDLE;
            end
        endcase
    end
end

endmodule
