// The device bridge, with the devices of phase B, behind the bus of the CPU, and a
// serial line on the UART, for the ISS comparison testbench (tb_b32p4.v compiled with
// the macro DEVICES). Include it inside the testbench module, before the instance that
// connects to the bus: it declares the bus signals, bus_request to bus_read_word, and
// device_interrupts. The testbench must have `clk`, `reset`, the ROM as
// `reg [31:0] rom [0:1023]`, `cycle` (an integer, the number of the current cycle)
// and INTERRUPTS.
//
// The timer is interrupt 1 and the UART interrupt 2. The DIP switches are 0101. The
// ROM's data port gives the word a cycle after its address, as the BRAM's second port
// will.
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
