// Device bridge
// Answers bus requests for the ROM's data port and the devices on the FPGC
module device_bridge #(
    parameter CLOCKS_PER_BIT = 100  // of the UART
) (
    input  wire        clk,
    input  wire        reset,

    // Bus from the CPU
    input  wire        bus_request,
    input  wire        bus_write,
    input  wire [31:0] bus_addr,
    input  wire [31:0] bus_write_data,
    input  wire [3:0]  bus_byte_enable,
    output reg         bus_ready,
    output reg         bus_error,
    output reg  [31:0] bus_read_word,

    // ROM data port
    output wire [11:2] rom_addr,
    input  wire [31:0] rom_read_word,  // a cycle after rom_addr

    // Pins
    input  wire        uart_rx,
    output wire        uart_tx,
    output wire        user_led,
    input  wire [3:0]  switches,

    // Interrupts
    output wire        timer_irq,
    output wire        uart_irq
);

// Device registers
localparam UART_TRANSMIT    = 32'hFFFF8000;
localparam UART_RECEIVE     = 32'hFFFF8004;
localparam UART_STATUS      = 32'hFFFF8008;
localparam UART_CONTROL     = 32'hFFFF800C;
localparam COUNTER_SNAPSHOT = 32'hFFFF8100;
localparam COUNTER_LOW      = 32'hFFFF8104;
localparam COUNTER_HIGH     = 32'hFFFF8108;
localparam TIMER_COUNT      = 32'hFFFF8200;
localparam TIMER_CONTROL    = 32'hFFFF8204;
localparam TIMER_STATUS     = 32'hFFFF8208;
localparam BOARD_LED        = 32'hFFFF8300;
localparam BOARD_SWITCHES   = 32'hFFFF8304;

wire rom  = bus_addr[31:12] == 20'h04000;
wire word = bus_byte_enable == 4'b1111;

assign rom_addr = bus_addr[11:2];

// To the devices, a cycle after the access
reg        uart_transmit;
reg        uart_receive;
reg        uart_clear;
reg        uart_configure;
reg        counter_snapshot;
reg        timer_start;
reg        timer_configure;
reg        timer_clear;
reg        board_set_led;
reg [31:0] data;

// From the devices
wire        uart_tx_full;
wire        uart_received;
wire [7:0]  uart_rx_byte;
wire        uart_rx_waiting;
wire        uart_overflow;
wire        uart_framing_error;
wire        uart_irq_enable;
wire [63:0] snapshot;
wire [31:0] timer_count;
wire        timer_periodic;
wire        timer_irq_enable;
wire        timer_expired;
wire [3:0]  board_switches;

// Answer
reg waiting;  // the access waits for the ROM or the UART

always @(posedge clk) begin
    bus_ready        <= 1'b0;
    bus_error        <= 1'b0;
    bus_read_word    <= 32'd0;
    waiting          <= 1'b0;
    uart_transmit    <= 1'b0;
    uart_receive     <= 1'b0;
    uart_clear       <= 1'b0;
    uart_configure   <= 1'b0;
    counter_snapshot <= 1'b0;
    timer_start      <= 1'b0;
    timer_configure  <= 1'b0;
    timer_clear      <= 1'b0;
    board_set_led    <= 1'b0;
    data             <= bus_write_data;

    if (!reset && (bus_request || waiting)) begin
        bus_ready <= 1'b1;
        if (rom && !bus_write) begin
            // The ROM's word comes a cycle after its address
            if (waiting) begin
                bus_read_word <= rom_read_word;
            end else begin
                bus_ready <= 1'b0;
                waiting   <= 1'b1;
            end
        end else if (!word) begin
            bus_error <= 1'b1;
        end else if (bus_write) begin
            case (bus_addr)
                UART_TRANSMIT: begin
                    // Waits while the buffer is full
                    bus_ready     <= !uart_tx_full;
                    waiting       <= uart_tx_full;
                    uart_transmit <= !uart_tx_full;
                end
                UART_RECEIVE:     ;
                UART_STATUS:      uart_clear       <= 1'b1;
                UART_CONTROL:     uart_configure   <= 1'b1;
                COUNTER_SNAPSHOT: counter_snapshot <= 1'b1;
                COUNTER_LOW:      ;
                COUNTER_HIGH:     ;
                TIMER_COUNT:      timer_start      <= 1'b1;
                TIMER_CONTROL:    timer_configure  <= 1'b1;
                TIMER_STATUS:     timer_clear      <= 1'b1;
                BOARD_LED:        board_set_led    <= 1'b1;
                BOARD_SWITCHES:   ;
                default:          bus_error        <= 1'b1;
            endcase
        end else begin
            case (bus_addr)
                UART_TRANSMIT:    ;
                UART_RECEIVE: begin
                    // Waits for the byte from the buffer, 0 when it's empty
                    if (waiting && uart_received) begin
                        bus_read_word <= {24'd0, uart_rx_byte};
                    end else if (waiting || uart_rx_waiting) begin
                        bus_ready    <= 1'b0;
                        waiting      <= 1'b1;
                        uart_receive <= !waiting;
                    end
                end
                UART_STATUS:      bus_read_word <= {28'd0, uart_framing_error, uart_overflow,
                                                    uart_tx_full, uart_rx_waiting};
                UART_CONTROL:     bus_read_word <= {31'd0, uart_irq_enable};
                COUNTER_SNAPSHOT: ;
                COUNTER_LOW:      bus_read_word <= snapshot[31:0];
                COUNTER_HIGH:     bus_read_word <= snapshot[63:32];
                TIMER_COUNT:      bus_read_word <= timer_count;
                TIMER_CONTROL:    bus_read_word <= {30'd0, timer_irq_enable, timer_periodic};
                TIMER_STATUS:     bus_read_word <= {31'd0, timer_expired};
                BOARD_LED:        bus_read_word <= {31'd0, user_led};
                BOARD_SWITCHES:   bus_read_word <= {28'd0, board_switches};
                default:          bus_error     <= 1'b1;
            endcase
        end
    end
end

// Devices
uart #(
    .CLOCKS_PER_BIT(CLOCKS_PER_BIT)
) uart (
    .clk(clk),
    .reset(reset),
    .transmit(uart_transmit),
    .tx_byte(data[7:0]),
    .tx_full(uart_tx_full),
    .receive(uart_receive),
    .received(uart_received),
    .rx_byte(uart_rx_byte),
    .rx_waiting(uart_rx_waiting),
    .clear(uart_clear),
    .overflow(uart_overflow),
    .framing_error(uart_framing_error),
    .configure(uart_configure),
    .irq_enable_in(data[0]),
    .irq_enable(uart_irq_enable),
    .irq(uart_irq),
    .rx(uart_rx),
    .tx(uart_tx)
);

cycle_counter cycle_counter (
    .clk(clk),
    .reset(reset),
    .take_snapshot(counter_snapshot),
    .snapshot(snapshot)
);

timer timer (
    .clk(clk),
    .reset(reset),
    .start(timer_start),
    .start_count(data),
    .configure(timer_configure),
    .periodic_in(data[0]),
    .irq_enable_in(data[1]),
    .clear(timer_clear),
    .count(timer_count),
    .periodic(timer_periodic),
    .irq_enable(timer_irq_enable),
    .expired(timer_expired),
    .irq(timer_irq)
);

board_io board (
    .clk(clk),
    .reset(reset),
    .set_led(board_set_led),
    .led_in(data[0]),
    .led(user_led),
    .switch_pins(switches),
    .switches(board_switches)
);

endmodule
