// A serial line at 1 Mbaud on a UART, for testbenches. Include it inside the testbench
// module. It drives `uart_rx_line` (a wire it declares) and listens to `uart_tx_line`,
// which the testbench declares. The testbench must have `clk` and `cycle` (an integer,
// the number of the current cycle).
//
// Plusargs:
//
//   +uart_input=<file>   lines "c b": send the byte b (hex) as soon as cycle c has come
//                        and the byte before it is sent
//   +uart_output=<file>  the bytes from the UART, one per line in hex, with " bad" after
//                        a byte without its stop bit

localparam UART_BIT = 100;  // cycles per bit

reg [8*256-1:0] uart_path;
reg             uart_line = 1'b1;  // to the UART's rx
integer         uart_input_fd = 0;
integer         uart_output_fd = 0;

wire uart_rx_line = uart_line;

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
