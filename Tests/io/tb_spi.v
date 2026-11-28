// Testbench of the SPI controller, spi, with the flash model (flash_chip) on its pins,
// and a driver that plays the device bridge: it waits until the controller isn't busy
// before every strobe, and after a receive until its word is there.
//
// The flash starts with random content. The driver does random operations on it, and
// keeps a reference copy:
// page programs, sector erases (with the status polled until they're done), and reads
// with 03h, 0Bh and 6Bh from random addresses, with a random mix of byte and word
// transfers. Every byte read is checked. As software must, the driver keeps the chip
// select high for 5 cycles between commands (tSHSL after a program or erase is 50 ns).
//
// It also checks the controller's pins: the clock is low whenever the chip select
// changes, it's high for exactly HALF_PERIOD cycles and low for at least that (except
// at a reset), and a receive on one line sends 1s. The flash model checks the rest. It stops at the first
// error.
//
// The parameter HALF_PERIOD is 1, 2 or 4.
//
// Plusargs:
//
//   +operations=<n>  the number of operations (default 200)
//   +seed=<n>        the seed (default 0)
//   +resets=1        also resets the controller in the middle of some reads
//   +demo=1          instead: the JEDEC ID, a program of 8 bytes, and a quad read of them
//   +vcd=<file>      also dump a waveform
//
// At the end it writes "done: <reads> reads, <programs> programs, <erases> erases,
// <resets> resets".
`timescale 1ns / 1ps
module tb_spi;

parameter HALF_PERIOD = 1;

localparam SIZE = 1 << 16;  // the flash model has 64 KiB

reg clk = 1'b0;
always #5 clk = !clk;

reg reset = 1'b1;

// ---- The controller and the flash ----

reg         send = 1'b0;
reg         receive = 1'b0;
reg         word = 1'b0;
reg  [31:0] send_data = 32'd0;
wire [31:0] received;
wire        busy;
reg         configure = 1'b0;
reg         select_in = 1'b0;
reg         quad_in = 1'b0;
wire        select;
wire        quad;
wire        spi_clk;
wire        cs_n;
wire [3:0]  io_out;
wire [3:0]  io_enable;
wire [3:0]  io;
wire [3:0]  error;

spi #(
    .HALF_PERIOD(HALF_PERIOD),
    .QUAD(1)
) dut (
    .clk(clk),
    .reset(reset),
    .send(send),
    .receive(receive),
    .word(word),
    .send_data(send_data),
    .received(received),
    .busy(busy),
    .configure(configure),
    .select_in(select_in),
    .quad_in(quad_in),
    .select(select),
    .quad(quad),
    .irq_n(1'b1),
    .irq(),
    .spi_clk(spi_clk),
    .cs_n(cs_n),
    .io_out(io_out),
    .io_enable(io_enable),
    .io_in(io)
);

assign io[0] = io_enable[0] ? io_out[0] : 1'bz;
assign io[1] = io_enable[1] ? io_out[1] : 1'bz;
assign io[2] = io_enable[2] ? io_out[2] : 1'bz;
assign io[3] = io_enable[3] ? io_out[3] : 1'bz;

flash_chip #(
    .ADDRESS_BITS(16),
    .PROGRAM_TIME(2_000_000),
    .ERASE_TIME(5_000_000)
) chip (
    .cs_n(cs_n),
    .clk(spi_clk),
    .io(io),
    .error(error)
);

// ---- Checks of the pins ----

reg  failed = 1'b0;
reg  receiving = 1'b0;  // a receive on one line is running
time clk_at = 0;

task fail(input [8*48-1:0] what);
    if (!failed) begin
        failed = 1'b1;
        $display("error at %0t ns: %0s", $time, what);
    end
endtask

always @(cs_n)
    if (spi_clk !== 1'b0 && !reset)
        fail("the chip select changes while the clock is high");

always @(spi_clk) begin
    if (spi_clk === 1'b0 && $time - clk_at != HALF_PERIOD * 10 && !reset)
        fail("the clock isn't high for HALF_PERIOD cycles");
    if (spi_clk === 1'b1 && $time - clk_at < HALF_PERIOD * 10)
        fail("the clock is low for less than HALF_PERIOD cycles");
    if (spi_clk === 1'b1 && receiving && !(io_enable[0] && io_out[0]))
        fail("a receive doesn't send 1s");
    clk_at = $time;
end

// ---- The driver ----

reg [7:0] memory [0:SIZE-1];  // the reference
integer   reads = 0;
integer   programs = 0;
integer   erases = 0;
integer   resets = 0;
integer   seed = 0;
integer   operations = 200;
reg       with_resets = 1'b0;
reg [7:0] bytes [0:63];
integer   i;

task idle;
    while (busy)
        @(negedge clk);
endtask

task set(input s, input q);
    begin
        idle;
        configure = 1'b1;
        select_in = s;
        quad_in   = q;
        @(negedge clk);
        configure = 1'b0;
        if (!s)
            repeat (5) @(negedge clk);
    end
endtask

task put(input w, input [31:0] data);
    begin
        idle;
        send      = 1'b1;
        word      = w;
        send_data = data;
        @(negedge clk);
        send = 1'b0;
    end
endtask

task get(input w, output [31:0] data);
    begin
        idle;
        receiving = !quad;
        receive   = 1'b1;
        word      = w;
        @(negedge clk);
        receive = 1'b0;
        idle;
        receiving = 1'b0;
        data      = received;
    end
endtask

// The command and a 3-byte address as one word: the command goes first
function [31:0] command(input [7:0] opcode, input [23:0] address);
    command = {address[7:0], address[15:8], address[23:16], opcode};
endfunction

task wait_done;
    reg [31:0] status;
    begin
        set(1'b1, 1'b0);
        put(1'b0, 32'h05);
        status = 32'd1;
        while (status[0])
            get(1'b0, status);
        set(1'b0, 1'b0);
    end
endtask

task check_id;
    reg [31:0] id;
    begin
        set(1'b1, 1'b0);
        put(1'b0, 32'h9F);
        get(1'b1, id);
        set(1'b0, 1'b0);
        if (id[23:0] !== 24'h1840EF)
            fail("the JEDEC ID");
    end
endtask

// Sends `count` bytes from `bytes`, or receives them into it, as words and bytes
task transfer(input sending, input integer count, input mix);
    integer k;
    reg [31:0] data;
    begin
        k = 0;
        while (k < count) begin
            if (count - k >= 4 && (!mix || $random(seed) % 2)) begin
                if (sending) begin
                    put(1'b1, {bytes[k + 3], bytes[k + 2], bytes[k + 1], bytes[k]});
                end else begin
                    get(1'b1, data);
                    {bytes[k + 3], bytes[k + 2], bytes[k + 1], bytes[k]} = data;
                end
                k = k + 4;
            end else begin
                if (sending) begin
                    put(1'b0, {24'd0, bytes[k]});
                end else begin
                    get(1'b0, data);
                    bytes[k] = data[7:0];
                end
                k = k + 1;
            end
        end
    end
endtask

task program_page(input [23:0] address, input integer count);
    integer k;
    begin
        for (k = 0; k < count; k = k + 1) begin
            bytes[k] = $random(seed);
            memory[address + k] = memory[address + k] & bytes[k];
        end
        set(1'b1, 1'b0);
        put(1'b0, 32'h06);
        set(1'b0, 1'b0);
        set(1'b1, 1'b0);
        put(1'b1, command(8'h02, address));
        transfer(1'b1, count, 1'b1);
        set(1'b0, 1'b0);
        wait_done;
        programs = programs + 1;
    end
endtask

task erase(input [23:0] address);
    integer k;
    begin
        for (k = 0; k < 4096; k = k + 1)
            memory[{address[15:12], 12'd0} + k] = 8'hFF;
        set(1'b1, 1'b0);
        put(1'b0, 32'h06);
        set(1'b0, 1'b0);
        set(1'b1, 1'b0);
        put(1'b1, command(8'h20, address));
        set(1'b0, 1'b0);
        wait_done;
        erases = erases + 1;
    end
endtask

// A read with 03h, 0Bh or 6Bh. With `cut`, a reset comes after that many cycles of
// the first receive, and nothing is checked.
task read(input [7:0] opcode, input [23:0] address, input integer count,
          input integer cut);
    integer k;
    reg [31:0] dummy;
    begin
        set(1'b1, 1'b0);
        put(1'b1, command(opcode, address));
        if (opcode == 8'h0B)
            get(1'b0, dummy);
        if (opcode == 8'h6B) begin
            set(1'b1, 1'b1);
            get(1'b1, dummy);
        end
        if (cut >= 0) begin
            idle;
            receive = 1'b1;
            word    = 1'b1;
            @(negedge clk);
            receive = 1'b0;
            repeat (cut) @(negedge clk);
            reset = 1'b1;
            @(negedge clk);
            reset = 1'b0;
            repeat (5) @(negedge clk);
            resets = resets + 1;
        end else begin
            transfer(1'b0, count, 1'b1);
            set(1'b0, 1'b0);
            for (k = 0; k < count; k = k + 1)
                if (bytes[k] !== memory[(address + k) % SIZE])
                    fail("a byte read");
            reads = reads + 1;
        end
    end
endtask

task random_operation;
    integer r;
    integer count;
    reg [23:0] address;
    reg [7:0] opcode;
    begin
        r       = $unsigned($random(seed)) % 100;
        address = $unsigned($random(seed)) % SIZE;
        count   = 1 + $unsigned($random(seed)) % 64;
        if (r < 3) begin
            erase(address);
        end else if (r < 40) begin
            if (address[7:0] + count > 256)
                count = 256 - address[7:0];
            program_page(address, count);
        end else begin
            opcode = r < 55 ? 8'h03 : r < 70 ? 8'h0B : 8'h6B;
            read(opcode, address, count,
                 with_resets && r % 8 == 0 ? $unsigned($random(seed)) % (70 * HALF_PERIOD)
                                           : -1);
        end
    end
endtask

reg [8*256-1:0] vcd;

initial begin
    if ($value$plusargs("vcd=%s", vcd)) begin
        $dumpfile(vcd);
        $dumpvars(0, tb_spi);
    end
    void'($value$plusargs("seed=%d", seed));
    void'($value$plusargs("operations=%d", operations));
    with_resets = $test$plusargs("resets");
    // Random content, after the model's own start
    #1;
    for (i = 0; i < SIZE; i = i + 1) begin
        memory[i]      = $random(seed);
        chip.memory[i] = memory[i];
    end
    repeat (3) @(negedge clk);
    reset = 1'b0;
    @(negedge clk);

    check_id;
    if ($test$plusargs("demo")) begin
        program_page(24'h000100, 8);
        read(8'h6B, 24'h000100, 8, -1);
    end else begin
        for (i = 0; i < operations && !failed && error == 4'd0; i = i + 1)
            random_operation;
    end
    repeat (10) @(negedge clk);
    $display("done: %0d reads, %0d programs, %0d erases, %0d resets", reads, programs,
             erases, resets);
    $finish;
end

endmodule
