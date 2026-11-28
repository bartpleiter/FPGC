// Testbench of the flash model, flash_chip: it plays the pins from a file of steps, and
// writes the four data lines as they are just before each step.
//
// Plusargs:
//
//   +steps=<file>    lines "<delay> <cs_n> <clk> <io_out> <io_enable>": after <delay> ps,
//                    set the pins (the delay in decimal, the rest in hex)
//   +outputs=<file>  one line per step: the data lines as 4 bits, IO3 first
//
// The chip is 64 KiB here, with programs of 1 us and erases of 2 us.
`timescale 1ps / 1ps
module tb_flash_chip;

reg         cs_n = 1'b1;
reg         clk = 1'b0;
reg  [3:0]  io_out = 4'd0;
reg  [3:0]  io_enable = 4'd0;
wire [3:0]  io;
wire [3:0]  error;

assign io[0] = io_enable[0] ? io_out[0] : 1'bz;
assign io[1] = io_enable[1] ? io_out[1] : 1'bz;
assign io[2] = io_enable[2] ? io_out[2] : 1'bz;
assign io[3] = io_enable[3] ? io_out[3] : 1'bz;

flash_chip #(
    .ADDRESS_BITS(16),
    .PROGRAM_TIME(1_000_000),
    .ERASE_TIME(2_000_000)
) chip (
    .cs_n(cs_n),
    .clk(clk),
    .io(io),
    .error(error)
);

reg [8*256-1:0] path;
integer         steps_fd;
integer         outputs_fd;
integer         delay;
reg             next_cs_n;
reg             next_clk;
reg  [3:0]      next_out;
reg  [3:0]      next_enable;

initial begin
    if (!$value$plusargs("steps=%s", path)) $fatal(1, "no +steps");
    steps_fd = $fopen(path, "r");
    if (!$value$plusargs("outputs=%s", path)) $fatal(1, "no +outputs");
    outputs_fd = $fopen(path, "w");
    while ($fscanf(steps_fd, "%d %h %h %h %h\n", delay, next_cs_n, next_clk, next_out,
                   next_enable) == 5 && error == 4'd0) begin
        #(delay);
        $fwrite(outputs_fd, "%b\n", io);
        cs_n      = next_cs_n;
        clk       = next_clk;
        io_out    = next_out;
        io_enable = next_enable;
    end
    #100_000;
    $fclose(outputs_fd);
    $display("done");
    $finish;
end

endmodule
