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

    // SPI flash 1 and 2, with four data lines
    output wire        flash1_clk,
    output wire        flash1_cs_n,
    output wire [3:0]  flash1_io_out,
    output wire [3:0]  flash1_io_enable,
    input  wire [3:0]  flash1_io_in,
    output wire        flash2_clk,
    output wire        flash2_cs_n,
    output wire [3:0]  flash2_io_out,
    output wire [3:0]  flash2_io_enable,
    input  wire [3:0]  flash2_io_in,

    // USB 1 and 2 (CH376T)
    output wire        usb1_clk,
    output wire        usb1_cs_n,
    output wire        usb1_mosi,
    input  wire        usb1_miso,
    input  wire        usb1_irq_n,
    output wire        usb2_clk,
    output wire        usb2_cs_n,
    output wire        usb2_mosi,
    input  wire        usb2_miso,
    input  wire        usb2_irq_n,

    // Ethernet (ENC28J60)
    output wire        eth_clk,
    output wire        eth_cs_n,
    output wire        eth_mosi,
    input  wire        eth_miso,
    input  wire        eth_irq_n,

    // SD card
    output wire        sd_clk,
    output wire        sd_cs_n,
    output wire        sd_mosi,
    input  wire        sd_miso,

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

// SPI registers, in each bus's 256 bytes from 0xFFFF8400
localparam SPI_BYTE    = 8'h00;
localparam SPI_WORD    = 8'h04;
localparam SPI_CONTROL = 8'h08;
localparam SPI_STATUS  = 8'h0C;

// The SPI buses' numbers, in the address from 0xFFFF8400
localparam FLASH1 = 0;
localparam FLASH2 = 1;
localparam USB1   = 2;
localparam USB2   = 3;
localparam ETH    = 4;
localparam SD     = 5;

wire rom  = bus_addr[31:12] == 20'h04000;
wire word = bus_byte_enable == 4'b1111;
wire spi  = bus_addr[31:12] == 20'hFFFF8 && bus_addr[11:8] >= 4'd4 && bus_addr[11:8] <= 4'd9;

wire [2:0] n = bus_addr[10:8] - 3'd4;  // the SPI bus, FLASH1 to SD

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
reg [5:0]  spi_send;       // per SPI bus, by its number
reg [5:0]  spi_receive;
reg [5:0]  spi_configure;
reg        spi_word;
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
wire [5:0]  spi_busy;
wire [191:0] spi_received;
wire [5:0]  spi_select;
wire [5:0]  spi_quad;
wire [5:0]  spi_irq;

// Answer
reg waiting;      // the access waits for the ROM, the UART or an SPI bus
reg spi_started;  // the receive of an SPI read has started

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
    spi_send         <= 6'd0;
    spi_receive      <= 6'd0;
    spi_configure    <= 6'd0;
    spi_word         <= bus_addr[2];
    data             <= bus_write_data;
    if (reset)
        spi_started <= 1'b0;

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
        end else if (spi) begin
            // Every access waits until the bus's last transfer is done
            case (bus_addr[7:0])
                SPI_BYTE, SPI_WORD:
                    if (bus_write) begin
                        bus_ready   <= !spi_busy[n];
                        waiting     <= spi_busy[n];
                        spi_send[n] <= !spi_busy[n];
                    end else if (spi_started) begin
                        // Then until its own receive is done
                        bus_ready   <= !spi_busy[n];
                        waiting     <= spi_busy[n];
                        spi_started <= spi_busy[n];
                        if (!spi_busy[n])
                            bus_read_word <= spi_received[32*n +: 32];
                    end else begin
                        bus_ready      <= 1'b0;
                        waiting        <= 1'b1;
                        spi_receive[n] <= !spi_busy[n];
                        spi_started    <= !spi_busy[n];
                    end
                SPI_CONTROL:
                    if (bus_write) begin
                        bus_ready        <= !spi_busy[n];
                        waiting          <= spi_busy[n];
                        spi_configure[n] <= !spi_busy[n];
                    end else begin
                        bus_read_word <= {30'd0, spi_quad[n], spi_select[n]};
                    end
                SPI_STATUS:
                    if (!bus_write)
                        bus_read_word <= {31'd0, spi_irq[n]};
                default:
                    bus_error <= 1'b1;
            endcase
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

// SPI buses, with the half period of their clock in cycles: 50 MHz for the flashes,
// 12.5 MHz for USB and Ethernet, 25 MHz for the SD card
spi #(
    .HALF_PERIOD(1),
    .QUAD(1)
) flash1 (
    .clk(clk),
    .reset(reset),
    .send(spi_send[FLASH1]),
    .receive(spi_receive[FLASH1]),
    .word(spi_word),
    .send_data(data),
    .received(spi_received[32*FLASH1 +: 32]),
    .busy(spi_busy[FLASH1]),
    .configure(spi_configure[FLASH1]),
    .select_in(data[0]),
    .quad_in(data[1]),
    .select(spi_select[FLASH1]),
    .quad(spi_quad[FLASH1]),
    .irq_n(1'b1),
    .irq(spi_irq[FLASH1]),
    .spi_clk(flash1_clk),
    .cs_n(flash1_cs_n),
    .io_out(flash1_io_out),
    .io_enable(flash1_io_enable),
    .io_in(flash1_io_in)
);

spi #(
    .HALF_PERIOD(1),
    .QUAD(1)
) flash2 (
    .clk(clk),
    .reset(reset),
    .send(spi_send[FLASH2]),
    .receive(spi_receive[FLASH2]),
    .word(spi_word),
    .send_data(data),
    .received(spi_received[32*FLASH2 +: 32]),
    .busy(spi_busy[FLASH2]),
    .configure(spi_configure[FLASH2]),
    .select_in(data[0]),
    .quad_in(data[1]),
    .select(spi_select[FLASH2]),
    .quad(spi_quad[FLASH2]),
    .irq_n(1'b1),
    .irq(spi_irq[FLASH2]),
    .spi_clk(flash2_clk),
    .cs_n(flash2_cs_n),
    .io_out(flash2_io_out),
    .io_enable(flash2_io_enable),
    .io_in(flash2_io_in)
);

// On the buses with one data line, IO0 is MOSI and IO1 is MISO
// verilator lint_off UNUSEDSIGNAL
wire [3:0] usb1_io_out;
wire [3:0] usb2_io_out;
wire [3:0] eth_io_out;
wire [3:0] sd_io_out;
// verilator lint_on UNUSEDSIGNAL

assign usb1_mosi = usb1_io_out[0];
assign usb2_mosi = usb2_io_out[0];
assign eth_mosi  = eth_io_out[0];
assign sd_mosi   = sd_io_out[0];

/* verilator lint_off PINCONNECTEMPTY */
spi #(
    .HALF_PERIOD(4),
    .QUAD(0)
) usb1 (
    .clk(clk),
    .reset(reset),
    .send(spi_send[USB1]),
    .receive(spi_receive[USB1]),
    .word(spi_word),
    .send_data(data),
    .received(spi_received[32*USB1 +: 32]),
    .busy(spi_busy[USB1]),
    .configure(spi_configure[USB1]),
    .select_in(data[0]),
    .quad_in(data[1]),
    .select(spi_select[USB1]),
    .quad(spi_quad[USB1]),
    .irq_n(usb1_irq_n),
    .irq(spi_irq[USB1]),
    .spi_clk(usb1_clk),
    .cs_n(usb1_cs_n),
    .io_out(usb1_io_out),
    .io_enable(),
    .io_in({2'b11, usb1_miso, 1'b1})
);

spi #(
    .HALF_PERIOD(4),
    .QUAD(0)
) usb2 (
    .clk(clk),
    .reset(reset),
    .send(spi_send[USB2]),
    .receive(spi_receive[USB2]),
    .word(spi_word),
    .send_data(data),
    .received(spi_received[32*USB2 +: 32]),
    .busy(spi_busy[USB2]),
    .configure(spi_configure[USB2]),
    .select_in(data[0]),
    .quad_in(data[1]),
    .select(spi_select[USB2]),
    .quad(spi_quad[USB2]),
    .irq_n(usb2_irq_n),
    .irq(spi_irq[USB2]),
    .spi_clk(usb2_clk),
    .cs_n(usb2_cs_n),
    .io_out(usb2_io_out),
    .io_enable(),
    .io_in({2'b11, usb2_miso, 1'b1})
);

spi #(
    .HALF_PERIOD(4),
    .QUAD(0)
) eth (
    .clk(clk),
    .reset(reset),
    .send(spi_send[ETH]),
    .receive(spi_receive[ETH]),
    .word(spi_word),
    .send_data(data),
    .received(spi_received[32*ETH +: 32]),
    .busy(spi_busy[ETH]),
    .configure(spi_configure[ETH]),
    .select_in(data[0]),
    .quad_in(data[1]),
    .select(spi_select[ETH]),
    .quad(spi_quad[ETH]),
    .irq_n(eth_irq_n),
    .irq(spi_irq[ETH]),
    .spi_clk(eth_clk),
    .cs_n(eth_cs_n),
    .io_out(eth_io_out),
    .io_enable(),
    .io_in({2'b11, eth_miso, 1'b1})
);

spi #(
    .HALF_PERIOD(2),
    .QUAD(0)
) sd (
    .clk(clk),
    .reset(reset),
    .send(spi_send[SD]),
    .receive(spi_receive[SD]),
    .word(spi_word),
    .send_data(data),
    .received(spi_received[32*SD +: 32]),
    .busy(spi_busy[SD]),
    .configure(spi_configure[SD]),
    .select_in(data[0]),
    .quad_in(data[1]),
    .select(spi_select[SD]),
    .quad(spi_quad[SD]),
    .irq_n(1'b1),
    .irq(spi_irq[SD]),
    .spi_clk(sd_clk),
    .cs_n(sd_cs_n),
    .io_out(sd_io_out),
    .io_enable(),
    .io_in({2'b11, sd_miso, 1'b1})
);
/* verilator lint_on PINCONNECTEMPTY */

endmodule
