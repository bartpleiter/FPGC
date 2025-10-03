/*
 * MemoryUnit (MU)
 * Single interface between CPU and slow Memory or IO
 * Optimized for simplicity, not speed, as high speed memory should be directly connected to CPU
 */
module MemoryUnit(
    //========================
    // System interface
    //========================
    input  wire         clk, // Assumed to be 50MHz
    input  wire         reset,

    //========================
    // CPU interface (50 MHz domain)
    //========================
    input  wire         start,
    input  wire [31:0]  addr, // Address in CPU words
    input  wire [31:0]  data,
    input  wire         we,
    output reg  [31:0]  q = 32'd0,
    output reg          done = 1'b0,

    //========================
    // I/O signals
    //========================
    // UART
    input wire          uart_rx,
    output wire         uart_tx,
    output wire         uart_irq,

    // Timer 1 interrupt
    output wire         OST1_int
);

//========================
// IO Devices
//========================
// UART TX
reg uart_tx_start = 1'b0;
wire uart_tx_done;
reg [7:0] uart_tx_data = 8'd0;

UARTtx uart_tx_controller(
.i_Clock    (clk),
.reset      (reset),
.i_Tx_DV    (uart_tx_start),
.i_Tx_Byte  (uart_tx_data),
.o_Tx_Active(),
.o_Tx_Serial(uart_tx),
.o_Tx_Done  (uart_tx_done)
);

// UART RX
wire [7:0] uart_rx_q;

UARTrx uart_rx_controller(
.i_Clock    (clk),
.reset      (reset),
.i_Rx_Serial(uart_rx),
.o_Rx_DV    (uart_irq),
.o_Rx_Byte  (uart_rx_q)
);

// OS timer 1
reg OST1_trigger = 1'b0;
reg OST1_set = 1'b0;
reg [31:0] OST1_value = 32'd0;

OStimer OST1(
.clk        (clk),
.reset      (reset),
.timerValue (OST1_value),
.setValue   (OST1_set),
.trigger    (OST1_trigger),
.interrupt  (OST1_int)
);

//========================
// Memory Unit Logic
//========================
// Address mappings
localparam ADDR_UART_TX         = 32'h7000000; // UART tx
localparam ADDR_UART_RX         = 32'h7000001; // UART rx
localparam ADDR_TIMER1_VALUE    = 32'h7000002; // Timer 1 value
localparam ADDR_TIMER1_START    = 32'h7000003; // Timer 1 start

localparam ADDR_OOB             = 32'h7000004; // All addresses >= this are out of bounds and return a constant

// State machine states
localparam STATE_IDLE           = 8'd0;
localparam STATE_RETURN_ZERO            = 8'd1;
localparam STATE_WAIT_UART_TX   = 8'd2;
localparam STATE_WAIT_UART_RX   = 8'd3;
localparam STATE_WAIT_TIMER1_VALUE   = 8'd4;
localparam STATE_WAIT_TIMER1_START   = 8'd5;
reg [7:0] state = 8'd0;

always @(posedge clk) begin
    if (reset)
    begin
        state <= STATE_IDLE;
        q = 32'd0;
        done <= 1'b0;

        uart_tx_start <= 1'b0;
        uart_tx_data <= 8'd0;

        OST1_trigger <= 1'b0;
        OST1_set <= 1'b0;
        OST1_value <= 32'd0;
    end else
    begin
        case (state)
            STATE_IDLE:
            begin
                done <= 1'b0;
                q <= 32'd0;

                OST1_set <= 1'b0;
                OST1_trigger <= 1'b0;

                if (start)
                begin
                    if (addr == ADDR_UART_TX)
                    begin
                        // Set uart_tx signals
                        // We assume we is high, to prevent lockup if we is low
                        uart_tx_data <= data[7:0];
                        uart_tx_start <= 1'b1;
                        state <= STATE_WAIT_UART_TX;
                    end

                    if (addr == ADDR_UART_RX)
                    begin
                        // Set uart_rx signals
                        state <= STATE_WAIT_UART_RX;
                    end

                    if (addr == ADDR_TIMER1_VALUE)
                    begin
                        // We assume we is high, to prevent lockup if we is low
                        OST1_value <= data;
                        OST1_set <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    if (addr == ADDR_TIMER1_START)
                    begin
                        // We assume we is high, to prevent lockup if we is low
                        OST1_trigger <= 1'b1;
                        state <= STATE_RETURN_ZERO;
                    end

                    if (addr >= ADDR_OOB)
                    begin
                        // Out of range
                        state <= STATE_RETURN_ZERO;
                    end
                end
            end

            STATE_WAIT_UART_TX:
            begin
                uart_tx_start <= 1'b0;
                if (uart_tx_done)
                begin
                    done <= 1'b1;
                    state <= STATE_IDLE;
                end
            end

            STATE_WAIT_UART_RX:
            begin
                done <= 1'b1;
                q <= {24'd0, uart_rx_q};
                state <= STATE_IDLE;
            end

            STATE_RETURN_ZERO:
            begin
                // Out of bounds access
                done <= 1'b1;
                q <= 32'hDEADBEEF; // Indicate error
                state <= STATE_IDLE;
            end

            default:
            begin
                state <= STATE_IDLE;
                done <= 1'b0;
                q <= 32'd0;
            end
        endcase
    end
end


endmodule
