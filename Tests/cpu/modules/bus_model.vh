// A model of what's behind the bus of the CPU, for simulation only. Include it inside a
// testbench module, before the instance that connects to the bus: it declares the bus
// signals, bus_request to bus_read_word. The testbench must have `clk`, `reset`, the
// ROM as `reg [31:0] rom [0:1023]` (4 KiB at 0x04000000), `cycle` (an integer, the
// number of the current cycle), `sdram_latency` and SDRAM_RANDOM of
// sdram_line_model.vh, INTERRUPTS, and a task `stop` that ends the simulation.
//
// The bus has one access at a time. bus_request is 1 for one cycle, and the address,
// the write data and the byte enables stay the same until the answer: bus_ready is 1 for
// one cycle, at least one cycle after the request, with the word of a read on
// bus_read_word, and with bus_error for an address where nothing is. The model answers
// right in the next cycle, or with a random latency when the SDRAM's latency is random.
//
// Behind the bus:
//
//   - ROM reads give the ROM's words. (The CPU never puts a write to ROM on the bus.)
//   - The device registers, 0xFFFF8000 to 0xFFFFFEFF, are RAM that starts at 0, like
//     the I/O of the ISS in the comparison.
//   - Every other address behind the bus (the rest of 0x04000000 to 0x07FFFFFF) has
//     nothing: the answer has bus_error, a read gives 0, and a write is ignored.
//
// It checks the protocol: no request before the last one is answered, no X on the bus,
// only addresses behind the bus, and the same address, data and byte enables until the
// answer. A protocol error ends
// the run with a "protocol error" line.
//
// The model has no interrupts: device_interrupts is 0. Its task bus_finish, which the
// testbench calls before it stops, has nothing to do.
//
// Plusargs: +seed=<n>, the seed of the random latencies (default 0).

// ---- The bus ----

wire        bus_request;
wire        bus_write;
wire [31:0] bus_addr;
wire [31:0] bus_write_data;
wire [3:0]  bus_byte_enable;
reg         bus_ready = 1'b0;
reg  [31:0] bus_read_word = 32'bx;
reg         bus_error = 1'bx;

wire [INTERRUPTS:1] device_interrupts = {INTERRUPTS{1'b0}};

task bus_finish;
    begin
    end
endtask

// ---- The model ----

bit [31:0] bus_ram [0:(1 << 13) - 1];

integer     bus_seed = 0;
reg         bus_busy = 1'b0;   // an access is waiting for its answer
integer     bus_left;          // the cycles until the answer
reg         bus_at_write;      // the access, as it was at the request
reg  [31:0] bus_at_addr;
reg  [31:0] bus_at_data;
reg  [3:0]  bus_at_enable;

initial
    if (!$value$plusargs("seed=%d", bus_seed))
        bus_seed = 0;

function [12:0] bus_index(input [31:0] addr);
    bus_index = addr[14:2];
endfunction

function bus_rom(input [31:0] addr);
    bus_rom = addr >= 32'h04000000 && addr < 32'h04001000;
endfunction

function bus_devices(input [31:0] addr);
    bus_devices = addr >= 32'hFFFF8000 && addr < 32'hFFFFFF00;
endfunction

function bus_behind(input [31:0] addr);
    bus_behind = addr[31:26] == 6'b000001 || bus_devices(addr);
endfunction

// The extra cycles before an answer: none for most accesses, 1 to 7 for some
function integer bus_latency(input [31:0] r);
    if (sdram_latency != SDRAM_RANDOM || r[1:0] != 2'd0)
        bus_latency = 0;
    else
        bus_latency = 1 + r[4:2] % 7;
endfunction

task bus_protocol_error(input [8*64-1:0] what);
    begin
        $display("protocol error in cycle %0d: %0s", cycle, what);
        stop;
    end
endtask

// Does the access and answers it
task bus_answer;
    integer i;
    begin
        bus_error <= 1'b0;
        if (bus_rom(bus_at_addr)) begin
            bus_read_word <= rom[bus_at_addr[11:2]];
        end else if (!bus_devices(bus_at_addr)) begin
            bus_error     <= 1'b1;
            bus_read_word <= 32'd0;
        end else if (bus_at_write) begin
            for (i = 0; i < 4; i = i + 1)
                if (bus_at_enable[i])
                    bus_ram[bus_index(bus_at_addr)][8*i +: 8] = bus_at_data[8*i +: 8];
        end else begin
            bus_read_word <= bus_ram[bus_index(bus_at_addr)];
        end
        bus_ready <= 1'b1;
        bus_busy  = 1'b0;
    end
endtask

always @(posedge clk) begin
    bus_ready     <= 1'b0;
    bus_error     <= 1'bx;
    bus_read_word <= 32'bx;
    if (reset) begin
        bus_busy = 1'b0;
    end else begin
        if (bus_request === 1'bx)
            bus_protocol_error("bus_request is X");
        if (bus_busy && (bus_write !== bus_at_write || bus_addr !== bus_at_addr ||
                         bus_write_data !== bus_at_data ||
                         bus_byte_enable !== bus_at_enable))
            bus_protocol_error("the access changed before its answer");
        if (bus_busy && bus_left > 0) begin
            bus_left = bus_left - 1;
            if (bus_left == 0)
                bus_answer;
        end
        if (bus_request) begin
            if (bus_busy)
                bus_protocol_error("a bus request before the answer to the last one");
            if (bus_write && bus_rom(bus_addr))
                bus_protocol_error("a write to the ROM on the bus");
            if (!bus_behind(bus_addr))
                bus_protocol_error("an address that isn't behind the bus");
            if (^{bus_write, bus_addr, bus_byte_enable} === 1'bx ||
                (bus_write && ^bus_write_data === 1'bx))
                bus_protocol_error("an access with X on the bus");
            bus_busy      = 1'b1;
            bus_at_write  = bus_write;
            bus_at_addr   = bus_addr;
            bus_at_data   = bus_write_data;
            bus_at_enable = bus_byte_enable;
            bus_left      = bus_latency($random(bus_seed));
            if (bus_left == 0)
                bus_answer;
        end
    end
end
