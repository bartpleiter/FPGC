/*
 * Testbench for the SDRAM controller (not MIG 7).
 */
`timescale 1ns / 1ps

`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/SDRAMcontroller.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/mt48lc16m16a2.v"


module sdram_tb ();

reg clk = 1'b0;
reg reset = 1'b0;

//---------------------------SDRAM---------------------------------
// SDRAM signals
wire             SDRAM_CLK;     // SDRAM clock
wire    [31 : 0] SDRAM_DQ;      // SDRAM I/O
wire    [12 : 0] SDRAM_A;       // SDRAM Address
wire    [1 : 0]  SDRAM_BA;      // Bank Address
wire             SDRAM_CKE;     // Synchronous Clock Enable
wire             SDRAM_CSn;     // CS#
wire             SDRAM_RASn;    // RAS#
wire             SDRAM_CASn;    // CAS#
wire             SDRAM_WEn;     // WE#
wire    [3 : 0]  SDRAM_DQM;     // Mask

assign SDRAM_CLK = ~clk;

mt48lc16m16a2 sdram1 (
.Dq     (SDRAM_DQ[15:0]), 
.Addr   (SDRAM_A), 
.Ba     (SDRAM_BA), 
.Clk    (SDRAM_CLK), 
.Cke    (SDRAM_CKE), 
.Cs_n   (SDRAM_CSn), 
.Ras_n  (SDRAM_RASn), 
.Cas_n  (SDRAM_CASn), 
.We_n   (SDRAM_WEn), 
.Dqm    (SDRAM_DQM[1:0])
);

mt48lc16m16a2 sdram2 (
.Dq     (SDRAM_DQ[31:16]), 
.Addr   (SDRAM_A), 
.Ba     (SDRAM_BA), 
.Clk    (SDRAM_CLK), 
.Cke    (SDRAM_CKE), 
.Cs_n   (SDRAM_CSn), 
.Ras_n  (SDRAM_RASn), 
.Cas_n  (SDRAM_CASn), 
.We_n   (SDRAM_WEn), 
.Dqm    (SDRAM_DQM[3:2])
);

//---------------------------SDRAM Controller---------------------------------
// SDRAM controller signals
reg [20:0] cpu_addr = 21'b0;
reg [255:0] cpu_data = 256'b0;
reg cpu_we = 1'b0;
reg cpu_start = 1'b0;
wire cpu_done;
wire [255:0] cpu_q;

SDRAMcontroller sdc (
    // Clock and reset
    .clk(clk),
    .reset(reset),

    .cpu_addr(cpu_addr),
    .cpu_data(cpu_data),
    .cpu_we(cpu_we),
    .cpu_start(cpu_start),
    .cpu_done(cpu_done),
    .cpu_q(cpu_q),

    .SDRAM_CSn(SDRAM_CSn),
    .SDRAM_WEn(SDRAM_WEn),
    .SDRAM_CASn(SDRAM_CASn),
    .SDRAM_RASn(SDRAM_RASn),
    .SDRAM_A(SDRAM_A),
    .SDRAM_BA(SDRAM_BA),
    .SDRAM_DQM(SDRAM_DQM),
    .SDRAM_DQ(SDRAM_DQ)
);

// 100 MHz clock
always begin
    #5 clk = ~clk;
end

integer clk_counter = 0;
always @(posedge clk) begin
    clk_counter = clk_counter + 1;
    if (clk_counter == 10000) begin
        $display("Simulation finished.");
        $finish;
    end
end

initial
begin
    $dumpfile("Hardware/Vivado/FPGC.srcs/simulation/output/sdram.vcd");
    $dumpvars;

    cpu_addr = 21'b0;
    cpu_data = 256'b0;
    cpu_we = 1'b0;
    cpu_start = 1'b0;

    #150;

    // Write test
    cpu_addr = 21'd0;
    cpu_data = 256'hDEADBEEF_CAFEBABE_01234567_89ABCDEF_11223344_55667788_99AABBCC_DDEEFF00;
    cpu_we = 1'b1;
    cpu_start = 1'b1;

    #10;
    cpu_start = 1'b0;

end

endmodule
