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
`include "FPGA/Verilog/Memory/ROM.v"
`include "FPGA/Verilog/Memory/VRAM.v"


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


//---------------Memory----------------
// ROM 
wire [8:0] bus_i_rom_addr;
wire [31:0] bus_i_rom_q;
wire [8:0] bus_d_rom_addr;
wire [31:0] bus_d_rom_q;

ROM rom(
.clk            (clk100),
.addr_instr     (bus_i_rom_addr),
.q_instr        (bus_i_rom_q),
.addr_data      (bus_d_rom_addr),
.q_data         (bus_d_rom_q)
);

// SDRAM/L1 cache
wire [22:0] bus_i_sdram_addr;
wire        bus_i_sdram_start;
wire [31:0]  bus_i_sdram_q;
wire         bus_i_sdram_done;
wire         bus_i_sdram_ready;

wire [22:0] bus_d_sdram_addr;
wire [31:0] bus_d_sdram_data;
wire        bus_d_sdram_we;
wire        bus_d_sdram_start;
wire [31:0]  bus_d_sdram_q;
wire         bus_d_sdram_done;
wire         bus_d_sdram_ready;

variable_latency_instruction_memory mem(
.clk100         (clk100),
.reset          (reset),
.bus_i_sdram_addr (),
.bus_i_sdram_start (),
.bus_i_sdram_q   (),
.bus_l1i_done   (),
.bus_l1i_ready  ()
);

// VRAM32
wire [10:0] VRAM32_cpu_addr;
wire [31:0] VRAM32_cpu_d;
wire VRAM32_cpu_we;
wire [31:0] VRAM32_cpu_q;

VRAM #(
.WIDTH(32),
.WORDS(1056),
.ADDR_BITS(11),
.LIST("FPGA/Data/Simulation/vram32.list")
) vram32(
//CPU port
.cpu_clk    (clk100),
.cpu_d      (VRAM32_cpu_d),
.cpu_addr   (VRAM32_cpu_addr),
.cpu_we     (VRAM32_cpu_we),
.cpu_q      (VRAM32_cpu_q),

//GPU port
.gpu_clk    (),
.gpu_d      (),
.gpu_addr   (),
.gpu_we     (),
.gpu_q      ()
);

// VRAM8
wire [13:0] VRAM8_cpu_addr;
wire [7:0] VRAM8_cpu_d;
wire VRAM8_cpu_we;
wire [7:0] VRAM8_cpu_q;

VRAM #(
.WIDTH(8),
.WORDS(8194),
.ADDR_BITS(14),
.LIST("FPGA/Data/Simulation/vram8.list")
) vram8(
//CPU port
.cpu_clk    (clk100),
.cpu_d      (VRAM8_cpu_d),
.cpu_addr   (VRAM8_cpu_addr),
.cpu_we     (VRAM8_cpu_we),
.cpu_q      (VRAM8_cpu_q),

//GPU port
.gpu_clk    (),
.gpu_d      (),
.gpu_addr   (),
.gpu_we     (),
.gpu_q      ()
);

// VRAMspr
wire [7:0] VRAMspr_cpu_addr;
wire [8:0] VRAMspr_cpu_d;
wire VRAMspr_cpu_we;
wire [8:0] VRAMspr_cpu_q;

VRAM #(
.WIDTH(9),
.WORDS(256),
.ADDR_BITS(8),
.LIST("FPGA/Data/Simulation/vramSPR.list")
) vramspr(
//CPU port
.cpu_clk    (clk100),
.cpu_d      (VRAMspr_cpu_d),
.cpu_addr   (VRAMspr_cpu_addr),
.cpu_we     (VRAMspr_cpu_we),
.cpu_q      (VRAMspr_cpu_q),

//GPU port
.gpu_clk    (),
.gpu_d      (),
.gpu_addr   (),
.gpu_we     (),
.gpu_q      ()
);

// VRAMpx
wire [16:0] VRAMpx_cpu_addr;
wire [7:0] VRAMpx_cpu_d;
wire VRAMpx_cpu_we;
wire [7:0] VRAMpx_cpu_q;

VRAM #(
.WIDTH(8),
.WORDS(76800),
.ADDR_BITS(17),
.LIST("FPGA/Data/Simulation/vramPX.list")
) vrampx(
//CPU port
.cpu_clk    (clk100),
.cpu_d      (VRAMpx_cpu_d),
.cpu_addr   (VRAMpx_cpu_addr),
.cpu_we     (VRAMpx_cpu_we),
.cpu_q      (VRAMpx_cpu_q),

//GPU port
.gpu_clk    (),
.gpu_d      (),
.gpu_addr   (),
.gpu_we     (),
.gpu_q      ()
);

// TODO: dummy memory unit


//---------------CPU----------------
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

// ROM bus for instruction and data memory
.bus_i_rom_addr (bus_i_rom_addr),
.bus_i_rom_q    (bus_i_rom_q),
.bus_d_rom_addr (bus_d_rom_addr),
.bus_d_rom_q    (bus_d_rom_q),

// VRAM32
.VRAM32_cpu_addr (VRAM32_cpu_addr),
.VRAM32_cpu_d   (VRAM32_cpu_d),
.VRAM32_cpu_we  (VRAM32_cpu_we),
.VRAM32_cpu_q   (VRAM32_cpu_q),

// VRAM8
.VRAM8_cpu_addr (VRAM8_cpu_addr),
.VRAM8_cpu_d    (VRAM8_cpu_d),
.VRAM8_cpu_we   (VRAM8_cpu_we),
.VRAM8_cpu_q    (VRAM8_cpu_q),

// VRAMspr
.VRAMspr_cpu_addr (VRAMspr_cpu_addr),
.VRAMspr_cpu_d  (VRAMspr_cpu_d),
.VRAMspr_cpu_we (VRAMspr_cpu_we),
.VRAMspr_cpu_q  (VRAMspr_cpu_q),

// VRAMpx
.VRAMpx_cpu_addr (VRAMpx_cpu_addr),
.VRAMpx_cpu_d   (VRAMpx_cpu_d),
.VRAMpx_cpu_we  (VRAMpx_cpu_we),
.VRAMpx_cpu_q   (VRAMpx_cpu_q),

// Memory Unit
.bus_mu_addr    (),
.bus_mu_start   (),
.bus_mu_data    (),
.bus_mu_we      (),
.bus_mu_q       (),
.bus_mu_done    (),
.bus_mu_ready   (),

.int1           (1'b0),
.int2           (1'b0),
.int3           (1'b0),
.int4           (1'b0),
.int5           (1'b0),
.int6           (1'b0),
.int7           (1'b0),
.int8           (1'b0)

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
