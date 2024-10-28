// Testbench for the CPU, connected to dummy memory with variable latency

`timescale 1ns / 1ps

`include "FPGA/Verilog/CPU/CPU.v"
`include "FPGA/Verilog/CPU/ALU.v"
`include "FPGA/Verilog/CPU/ControlUnit.v"
`include "FPGA/Verilog/CPU/InstructionDecoder.v"
`include "FPGA/Verilog/CPU/Regbank.v"
`include "FPGA/Verilog/CPU/Stack.v"
`include "FPGA/Verilog/CPU/InstrMem.v"
`include "FPGA/Verilog/CPU/DataMem.v"
`include "FPGA/Verilog/CPU/Regr.v"
`include "FPGA/Verilog/CPU/IntController.v"
`include "FPGA/Verilog/Memory/L1Icache.v"
`include "FPGA/Verilog/Memory/L1Dcache.v"
`include "FPGA/Verilog/Memory/SRAM.v"
`include "FPGA/Verilog/Memory/ROM.v"


module variable_latency_instruction_memory
(
    input clk100,
    input reset,

    input [26:0] bus_i_sdram_addr,
    input bus_i_sdram_start,
    output [31:0] bus_i_sdram_q,
    output bus_l1i_done,
    output bus_l1i_ready
);

parameter LATENCY = 1;

reg [31:0] memory [0:511];
reg [31:0] data_out;
reg [3:0] counter;
reg ready;
reg done;

assign bus_i_sdram_q = data_out;
assign bus_l1i_done = done;
assign bus_l1i_ready = ready;

initial begin
    $readmemb("FPGA/Data/Simulation/rom.list", memory);
    data_out = 32'b0;
    counter = 0;
    ready = 0;
    done = 0;
end

localparam STATE_IDLE = 3'b000;
localparam STATE_WAIT = 3'b001;

reg [2:0] state = STATE_IDLE;

always @(posedge clk100)
begin
    if (reset)
    begin
        state <= STATE_IDLE;
        ready <= 0;
        done <= 0;
        counter <= 0;
    end
    else
    begin
        case (state)
            STATE_IDLE:
            begin
                ready <= 1;
                done <= 0;
                if (bus_i_sdram_start)
                begin
                    // Uncomment for cache hit simulation
                    data_out <= memory[bus_i_sdram_addr];
                    ready <= 1;
                    done <= 1;
                    state <= STATE_IDLE;

                    // Comment for cache hit simulation
                    // state <= STATE_WAIT;
                    // ready <= 0;
                    // done <= 0;
                    // counter <= 0;
                end
            end
            STATE_WAIT:
            begin
                if (counter < LATENCY)
                begin
                    counter <= counter + 1;
                    ready <= 0;
                    done <= 0;
                end
                else
                begin
                    counter <= 0;
                    data_out <= memory[bus_i_sdram_addr];
                    ready <= 1;
                    done <= 1;
                    state <= STATE_IDLE;
                end
            end
        endcase
    end
end

endmodule


module tb_cpu;

//Clock I/O
reg clk;
reg clk100;
reg reset;

//---------------CPU----------------
// CPU bus I/O
wire [26:0] bus_i_sdram_addr;
wire        bus_i_sdram_start;
wire [31:0]  bus_i_sdram_q;
wire         bus_i_sdram_done;
wire         bus_i_sdram_ready;

wire [26:0] bus_d_sdram_addr;
wire [31:0] bus_d_sdram_data;
wire        bus_d_sdram_we;
wire        bus_d_sdram_start;
wire [31:0]  bus_d_sdram_q;
wire         bus_d_sdram_done;
wire         bus_d_sdram_ready;

// ROM bus for instruction memory
wire [8:0] bus_i_rom_addr;
wire [31:0] bus_i_rom_q;

variable_latency_instruction_memory mem(
.clk100         (clk100),
.reset          (reset),
.bus_i_sdram_addr (bus_i_sdram_addr),
.bus_i_sdram_start (bus_i_sdram_start),
.bus_i_sdram_q   (bus_i_sdram_q),
.bus_l1i_done   (bus_i_sdram_done),
.bus_l1i_ready  (bus_i_sdram_ready)
);

CPU cpu(
.clk            (clk),
.clk100         (clk100),
.reset          (reset),

// SDRAM bus for instruction and data memory
.bus_i_sdram_addr (bus_i_sdram_addr),
.bus_i_sdram_start (bus_i_sdram_start),
.bus_i_sdram_q   (bus_i_sdram_q),
.bus_i_sdram_done (bus_i_sdram_done),
.bus_i_sdram_ready (bus_i_sdram_ready),

.bus_d_sdram_addr (bus_d_sdram_addr),
.bus_d_sdram_data (bus_d_sdram_data),
.bus_d_sdram_we  (bus_d_sdram_we),
.bus_d_sdram_start (bus_d_sdram_start),
.bus_d_sdram_q   (bus_d_sdram_q),
.bus_d_sdram_done (bus_d_sdram_done),
.bus_d_sdram_ready (bus_d_sdram_ready),

// ROM bus for instruction memory
.bus_i_rom_addr (bus_i_rom_addr),
.bus_i_rom_q    (bus_i_rom_q),

.int1           (1'b0),
.int2           (1'b0),
.int3           (1'b0),
.int4           (1'b0),
.int5           (1'b0),
.int6           (1'b0),
.int7           (1'b0),
.int8           (1'b0)

);

//---------------Memory----------------
// ROM 
ROM rom(
.clk            (clk100),
.reset          (reset),
.address        (bus_i_rom_addr),
.q              (bus_i_rom_q)
);



initial
begin
    $dumpfile("FPGA/Simulation/output/cpu.vcd");
    $dumpvars;

    clk = 0;
    clk100 = 0;
    reset = 0;

    repeat(3)
    begin
        #5 clk100 = ~clk100; clk = ~clk; //50MHz
        #5 clk100 = ~clk100; //100MHz
        #5 clk100 = ~clk100; clk = ~clk; //50MHz
        #5 clk100 = ~clk100; //100MHz
    end

    reset = 1;

    repeat(3)
    begin
        #5 clk100 = ~clk100; clk = ~clk; //50MHz
        #5 clk100 = ~clk100; //100MHz
        #5 clk100 = ~clk100; clk = ~clk; //50MHz
        #5 clk100 = ~clk100; //100MHz
    end

    reset = 0;

    repeat(1000)
    begin
        #5 clk100 = ~clk100; clk = ~clk; //50MHz
        #5 clk100 = ~clk100; //100MHz
        #5 clk100 = ~clk100; clk = ~clk; //50MHz
        #5 clk100 = ~clk100; //100MHz
    end

    #1 $finish;
end

endmodule
