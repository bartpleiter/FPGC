// The device bridge, with its devices, behind the bus of the CPU, and a serial line on
// the UART, for the ISS comparison testbench (tb_b32p4.v compiled with the macro
// DEVICES). Include it inside the testbench module, before the instance that
// connects to the bus: it declares the bus signals, bus_request to bus_read_word, and
// device_interrupts. The testbench must have `clk`, `reset`, the ROM as
// `reg [31:0] rom [0:1023]`, `cycle` (an integer, the number of the current cycle)
// and INTERRUPTS.
//
// The timer is interrupt 1 and the UART interrupt 2. The DIP switches are 0101. The
// ROM's data port gives the word a cycle after its address, as the BRAM's second port
// will. The flashes have an erased flash model each (flash_chip, 64 KiB, which the
// testbench includes) with pulled-up data lines, the other SPI buses' MISO is 1, and
// their interrupt pins are inactive.
//
// The serial line (serial_line.vh) runs at 1 Mbaud, as the UART, with its plusargs
// +uart_input and +uart_output. The task bus_finish, which the testbench calls before it
// stops, waits until the UART has sent everything.

// ---- The bus ----

wire        bus_request;
wire        bus_write;
wire [31:0] bus_addr;
wire [31:0] bus_write_data;
wire [3:0]  bus_byte_enable;
wire        bus_ready;
wire        bus_error;
wire [31:0] bus_read_word;

// ---- The serial line ----

wire uart_tx_line;

`include "serial_line.vh"

// ---- The device bridge, with the devices ----

wire [11:2] rom_addr;
reg  [31:0] rom_read_word = 32'd0;
wire        uart_irq;
wire        timer_irq;

wire [INTERRUPTS:1] device_interrupts = {uart_irq, timer_irq};

// ---- The flashes ----

wire        flash1_clk;
wire        flash1_cs_n;
wire [3:0]  flash1_io_out;
wire [3:0]  flash1_io_enable;
tri1 [3:0]  flash1_io;  // pulled up
wire        flash2_clk;
wire        flash2_cs_n;
wire [3:0]  flash2_io_out;
wire [3:0]  flash2_io_enable;
tri1 [3:0]  flash2_io;
wire        usb1_cs_n;
wire        usb2_cs_n;
wire        eth_cs_n;
wire        sd_cs_n;
wire [3:0]  flash1_error;
wire [3:0]  flash2_error;

genvar flash_line;
generate
    for (flash_line = 0; flash_line < 4; flash_line = flash_line + 1) begin : flash_lines
        assign flash1_io[flash_line] = flash1_io_enable[flash_line] ? flash1_io_out[flash_line] : 1'bz;
        assign flash2_io[flash_line] = flash2_io_enable[flash_line] ? flash2_io_out[flash_line] : 1'bz;
    end
endgenerate

flash_chip #(
    .ADDRESS_BITS(16)
) flash1 (
    .cs_n(flash1_cs_n),
    .clk(flash1_clk),
    .io(flash1_io),
    .error(flash1_error)
);

flash_chip #(
    .ADDRESS_BITS(16)
) flash2 (
    .cs_n(flash2_cs_n),
    .clk(flash2_clk),
    .io(flash2_io),
    .error(flash2_error)
);

always @(posedge clk)
    if (flash1_error != 4'd0 || flash2_error != 4'd0)
        $fatal(1, "a flash model reports a broken rule in cycle %0d", cycle);

device_bridge #(
    .CLOCKS_PER_BIT(100)
) bridge (
    .clk(clk),
    .reset(reset),
    .bus_request(bus_request),
    .bus_write(bus_write),
    .bus_addr(bus_addr),
    .bus_write_data(bus_write_data),
    .bus_byte_enable(bus_byte_enable),
    .bus_ready(bus_ready),
    .bus_error(bus_error),
    .bus_read_word(bus_read_word),
    .rom_addr(rom_addr),
    .rom_read_word(rom_read_word),
    .uart_rx(uart_rx_line),
    .uart_tx(uart_tx_line),
    .user_led(),
    .switches(4'b0101),
    .flash1_clk(flash1_clk),
    .flash1_cs_n(flash1_cs_n),
    .flash1_io_out(flash1_io_out),
    .flash1_io_enable(flash1_io_enable),
    .flash1_io_in(flash1_io),
    .flash2_clk(flash2_clk),
    .flash2_cs_n(flash2_cs_n),
    .flash2_io_out(flash2_io_out),
    .flash2_io_enable(flash2_io_enable),
    .flash2_io_in(flash2_io),
    .usb1_clk(),
    .usb1_cs_n(usb1_cs_n),
    .usb1_mosi(),
    .usb1_miso(1'b1),
    .usb1_irq_n(1'b1),
    .usb2_clk(),
    .usb2_cs_n(usb2_cs_n),
    .usb2_mosi(),
    .usb2_miso(1'b1),
    .usb2_irq_n(1'b1),
    .eth_clk(),
    .eth_cs_n(eth_cs_n),
    .eth_mosi(),
    .eth_miso(1'b1),
    .eth_irq_n(1'b1),
    .sd_clk(),
    .sd_cs_n(sd_cs_n),
    .sd_mosi(),
    .sd_miso(1'b1),
    .timer_irq(timer_irq),
    .uart_irq(uart_irq)
);

always @(posedge clk)
    rom_read_word <= rom[rom_addr];

task bus_finish;
    begin
        while (!bridge.uart.tx_empty || bridge.uart.tx_starting || bridge.uart.tx_busy)
            @(posedge clk);
        repeat (UART_BIT) @(posedge clk);
    end
endtask
