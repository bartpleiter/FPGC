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
// The serial line runs at 1 Mbaud, as the UART. It sends the bytes of +uart_input to
// the UART, and writes every byte that the UART sends to +uart_output. The task
// bus_finish, which the testbench calls before it stops, waits until the UART has sent
// everything.
//
// Plusargs:
//
//   +uart_input=<file>   lines "c b": send the byte b (hex) as soon as cycle c has come
//                        and the byte before it is sent
//   +uart_output=<file>  the bytes from the UART, one per line in hex, with " bad" after
//                        a byte without its stop bit

// ---- The bus ----

wire        bus_request;
wire        bus_write;
wire [31:0] bus_addr;
wire [31:0] bus_write_data;
wire [3:0]  bus_byte_enable;
wire        bus_ready;
wire        bus_error;
wire [31:0] bus_read_word;

// ---- The device bridge, with the devices ----

wire [11:2] rom_addr;
reg  [31:0] rom_read_word = 32'd0;
wire        uart_irq;
wire        timer_irq;
wire        uart_rx_line;
wire        uart_tx_line;

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
    .leds(),
    .switches(4'b0101),
    .timer_irq(timer_irq),
    .uart_irq(uart_irq)
);

always @(posedge clk)
    rom_read_word <= rom[rom_addr];

// ---- The serial line ----

localparam UART_BIT = 100;  // cycles per bit

reg [8*256-1:0] uart_path;
reg             uart_line = 1'b1;  // to the UART's rx
integer         uart_input_fd = 0;
integer         uart_output_fd = 0;

assign uart_rx_line = uart_line;

task uart_send(input [7:0] value);
    integer i;
    begin
        uart_line <= 1'b0;
        repeat (UART_BIT) @(posedge clk);
        for (i = 0; i < 8; i = i + 1) begin
            uart_line <= value[i];
            repeat (UART_BIT) @(posedge clk);
        end
        uart_line <= 1'b1;
        repeat (UART_BIT) @(posedge clk);
    end
endtask

// The sender
integer   uart_at;
reg [7:0] uart_value;

initial begin
    if ($value$plusargs("uart_input=%s", uart_path)) begin
        uart_input_fd = $fopen(uart_path, "r");
        if (uart_input_fd == 0)
            $fatal(1, "cannot read %0s", uart_path);
        while ($fscanf(uart_input_fd, "%d %h\n", uart_at, uart_value) == 2) begin
            while (cycle < uart_at)
                @(posedge clk);
            uart_send(uart_value);
        end
    end
end

// The receiver, which samples each bit in its middle
reg [7:0] uart_byte;
integer   uart_i;

initial begin
    if ($value$plusargs("uart_output=%s", uart_path))
        uart_output_fd = $fopen(uart_path, "w");
    forever begin
        @(negedge uart_tx_line);
        repeat (UART_BIT / 2) @(posedge clk);
        for (uart_i = 0; uart_i < 8; uart_i = uart_i + 1) begin
            repeat (UART_BIT) @(posedge clk);
            uart_byte[uart_i] = uart_tx_line;
        end
        repeat (UART_BIT) @(posedge clk);
        if (uart_output_fd != 0) begin
            if (uart_tx_line)
                $fwrite(uart_output_fd, "%02x\n", uart_byte);
            else
                $fwrite(uart_output_fd, "%02x bad\n", uart_byte);
            $fflush(uart_output_fd);
        end
    end
end

task bus_finish;
    begin
        while (!bridge.uart.tx_empty || bridge.uart.tx_starting || bridge.uart.tx_busy)
            @(posedge clk);
        repeat (UART_BIT) @(posedge clk);
    end
endtask
