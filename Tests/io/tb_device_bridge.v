// Testbench of device_bridge, with its devices, and the UART at 1 Mbaud (100 cycles
// per bit). See tb_unit.vh for how the vectors are applied. The vectors have the bus
// accesses, the rx line and the switches side by side. The ROM answers a cycle after
// its address, with the word C0DE0000 plus the word's number.
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
wire        bus_ready;
wire        bus_error;
wire [31:0] bus_read_word;
wire [11:2] rom_addr;
reg  [31:0] rom_read_word;
wire        uart_tx;
wire [4:0]  leds;
wire        timer_irq;
wire        uart_irq;

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
    .leds(leds),
    .switches(switches),
    .timer_irq(timer_irq),
    .uart_irq(uart_irq)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h %h\n", label, reset,
                   bus_request, bus_write, bus_addr, bus_write_data, bus_byte_enable,
                   uart_rx, switches) == 9)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h %h %h\n", bus_ready,
                               bus_error, bus_read_word, leds, timer_irq, uart_irq,
                               uart_tx);
    tb_close;
end

endmodule
