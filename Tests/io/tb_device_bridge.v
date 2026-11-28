// Testbench of device_bridge, with its devices, and the UART at 1 Mbaud (100 cycles
// per bit). See tb_unit.vh for how the vectors are applied. The vectors have the bus
// accesses, the rx line, the switches and the SPI buses' interrupt pins side by side.
// The ROM answers a cycle after its address, with the word C0DE0000 plus the word's
// number.
//
// The flashes have a flash model (flash_chip) each, 64 KiB, whose byte k is k * 3 + 1
// for flash 1 and k * 3 + 65 for flash 2. Their data lines are pulled up, so a line
// that nothing drives reads 1, and the other buses' MISO is 1. The vectors' spi_irq_n
// and the outputs' spi_cs_n have a bit per SPI bus, by its number: flash 1, flash 2,
// USB 1, USB 2, Ethernet, the SD card.
`include "flash_chip.v"
`timescale 1ns / 1ps
module tb_device_bridge;

`include "tb_unit.vh"

reg         reset;
reg         bus_request;
reg         bus_write;
reg  [31:0] bus_addr;
reg  [31:0] bus_write_data;
reg  [3:0]  bus_byte_enable;
reg         uart_rx;
reg  [3:0]  switches;
reg  [5:0]  spi_irq_n;
wire        bus_ready;
wire        bus_error;
wire [31:0] bus_read_word;
wire [11:2] rom_addr;
reg  [31:0] rom_read_word;
wire        uart_tx;
wire        user_led;
wire        timer_irq;
wire        uart_irq;
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

genvar line;
generate
    for (line = 0; line < 4; line = line + 1) begin : flash_lines
        assign flash1_io[line] = flash1_io_enable[line] ? flash1_io_out[line] : 1'bz;
        assign flash2_io[line] = flash2_io_enable[line] ? flash2_io_out[line] : 1'bz;
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
integer     k;

initial begin
    #1;
    for (k = 0; k < 1 << 16; k = k + 1) begin
        flash1.memory[k] = k * 3 + 1;
        flash2.memory[k] = k * 3 + 65;
    end
end

always @(posedge clk)
    if (flash1_error != 4'd0 || flash2_error != 4'd0)
        $fatal(1, "a flash model reports a broken rule");

always @(posedge clk)
    rom_read_word <= 32'hC0DE0000 | {22'd0, rom_addr};

device_bridge #(
    .CLOCKS_PER_BIT(100)
) dut (
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
    .uart_rx(uart_rx),
    .uart_tx(uart_tx),
    .user_led(user_led),
    .switches(switches),
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
    .usb1_irq_n(spi_irq_n[2]),
    .usb2_clk(),
    .usb2_cs_n(usb2_cs_n),
    .usb2_mosi(),
    .usb2_miso(1'b1),
    .usb2_irq_n(spi_irq_n[3]),
    .eth_clk(),
    .eth_cs_n(eth_cs_n),
    .eth_mosi(),
    .eth_miso(1'b1),
    .eth_irq_n(spi_irq_n[4]),
    .sd_clk(),
    .sd_cs_n(sd_cs_n),
    .sd_mosi(),
    .sd_miso(1'b1),
    .timer_irq(timer_irq),
    .uart_irq(uart_irq)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h %h %h\n", label, reset,
                   bus_request, bus_write, bus_addr, bus_write_data, bus_byte_enable,
                   uart_rx, switches, spi_irq_n) == 10)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h %h %h %h\n", bus_ready,
                               bus_error, bus_read_word, user_led, timer_irq, uart_irq,
                               uart_tx, {sd_cs_n, eth_cs_n, usb2_cs_n, usb1_cs_n,
                                         flash2_cs_n, flash1_cs_n});
    tb_close;
end

endmodule
