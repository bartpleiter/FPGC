/*
 * Latency Benchmark Testbench for FPGC Memory System
 * This testbench measures latency in different memory access scenarios:
 * 1. L1I cache hit (instruction fetch)
 * 2. L1I cache miss (instruction fetch from SDRAM)
 * 3. L1D cache hit read
 * 4. L1D cache miss read (clean line)
 * 5. L1D cache miss read (dirty line eviction)
 * 6. L1D cache hit write
 * 7. L1D cache miss write (clean line)
 * 8. L1D cache miss write (dirty line eviction)
 *
 * Measures cycles at each layer:
 * - CPU pipeline (50 MHz domain)
 * - Cache controller (100 MHz domain)
 * - SDRAM controller (100 MHz domain)
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/Memory/mt48lc16m16a2.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SDRAMcontroller.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/DPRAM.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/CacheControllerSDRAM.v"

module latency_benchmark_tb();

reg clk = 1'b0;
reg clk100 = 1'b1;
reg reset = 1'b0;

// SDRAM clock phase shift configuration
parameter SDRAM_CLK_PHASE = 270;
localparam real PHASE_DELAY = (SDRAM_CLK_PHASE / 360.0) * 10.0;

// SDRAM signals
reg              SDRAM_CLK_internal = 1'b0;
wire             SDRAM_CLK;
wire    [31 : 0] SDRAM_DQ;
wire    [12 : 0] SDRAM_A;
wire    [1 : 0]  SDRAM_BA;
wire             SDRAM_CKE;
wire             SDRAM_CSn;
wire             SDRAM_RASn;
wire             SDRAM_CASn;
wire             SDRAM_WEn;
wire    [3 : 0]  SDRAM_DQM;

assign SDRAM_CLK = SDRAM_CLK_internal;

always @(clk100) begin
    SDRAM_CLK_internal <= #PHASE_DELAY clk100;
end

//==================================================================
// SDRAM Models
//==================================================================

mt48lc16m16a2 #(
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/sdram.list"),
    .HIGH_HALF(0)
) sdram1 (
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

mt48lc16m16a2 #(
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/sdram.list"),
    .HIGH_HALF(1)
) sdram2 (
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

//==================================================================
// SDRAM Controller
//==================================================================

wire [20:0]     sdc_addr;
wire [255:0]    sdc_data;
wire            sdc_we;
wire            sdc_start;
wire            sdc_done;
wire [255:0]    sdc_q;

SDRAMcontroller sdc (
    .clk(clk100),
    .reset(reset),
    
    .cpu_addr(sdc_addr),
    .cpu_data(sdc_data),
    .cpu_we(sdc_we),
    .cpu_start(sdc_start),
    .cpu_done(sdc_done),
    .cpu_q(sdc_q),
    
    .SDRAM_CKE(SDRAM_CKE),
    .SDRAM_CSn(SDRAM_CSn),
    .SDRAM_WEn(SDRAM_WEn),
    .SDRAM_CASn(SDRAM_CASn),
    .SDRAM_RASn(SDRAM_RASn),
    .SDRAM_A(SDRAM_A),
    .SDRAM_BA(SDRAM_BA),
    .SDRAM_DQM(SDRAM_DQM),
    .SDRAM_DQ(SDRAM_DQ)
);

//==================================================================
// L1 Cache DPRAM
//==================================================================

// L1i cache DPRAM
wire [270:0] l1i_pipe_d = 271'd0;
wire [6:0]   l1i_pipe_addr;
wire         l1i_pipe_we = 1'b0;
wire [270:0] l1i_pipe_q;
wire [270:0] l1i_ctrl_d;
wire [6:0]   l1i_ctrl_addr;
wire         l1i_ctrl_we;
wire [270:0] l1i_ctrl_q;

DPRAM #(
    .WIDTH(271),
    .WORDS(128),
    .ADDR_BITS(7)
) l1i_ram (
    .clk_pipe(clk),
    .pipe_d(l1i_pipe_d),
    .pipe_addr(l1i_pipe_addr),
    .pipe_we(l1i_pipe_we),
    .pipe_q(l1i_pipe_q),
    .clk_ctrl(clk100),
    .ctrl_d(l1i_ctrl_d),
    .ctrl_addr(l1i_ctrl_addr),
    .ctrl_we(l1i_ctrl_we),
    .ctrl_q(l1i_ctrl_q)
);

// L1d cache DPRAM
wire [270:0] l1d_pipe_d = 271'd0;
wire [6:0]   l1d_pipe_addr;
wire         l1d_pipe_we = 1'b0;
wire [270:0] l1d_pipe_q;
wire [270:0] l1d_ctrl_d;
wire [6:0]   l1d_ctrl_addr;
wire         l1d_ctrl_we;
wire [270:0] l1d_ctrl_q;

DPRAM #(
    .WIDTH(271),
    .WORDS(128),
    .ADDR_BITS(7)
) l1d_ram (
    .clk_pipe(clk),
    .pipe_d(l1d_pipe_d),
    .pipe_addr(l1d_pipe_addr),
    .pipe_we(l1d_pipe_we),
    .pipe_q(l1d_pipe_q),
    .clk_ctrl(clk100),
    .ctrl_d(l1d_ctrl_d),
    .ctrl_addr(l1d_ctrl_addr),
    .ctrl_we(l1d_ctrl_we),
    .ctrl_q(l1d_ctrl_q)
);

//==================================================================
// Cache Controller
//==================================================================

// Test bench control signals for cache controller
reg         tb_FE2_start = 1'b0;
reg  [31:0] tb_FE2_addr = 32'd0;
reg         tb_FE2_flush = 1'b0;
wire        tb_FE2_done;
wire [31:0] tb_FE2_result;

reg         tb_EXMEM2_start = 1'b0;
reg  [31:0] tb_EXMEM2_addr = 32'd0;
reg  [31:0] tb_EXMEM2_data = 32'd0;
reg         tb_EXMEM2_we = 1'b0;
wire        tb_EXMEM2_done;
wire [31:0] tb_EXMEM2_result;

reg         tb_clear_cache = 1'b0;
wire        tb_clear_cache_done;

CacheController cache_controller (
    .clk100(clk100),
    .reset(reset),
    
    .cpu_FE2_start(tb_FE2_start),
    .cpu_FE2_addr(tb_FE2_addr),
    .cpu_FE2_flush(tb_FE2_flush),
    .cpu_FE2_done(tb_FE2_done),
    .cpu_FE2_result(tb_FE2_result),
    
    .cpu_EXMEM2_start(tb_EXMEM2_start),
    .cpu_EXMEM2_addr(tb_EXMEM2_addr),
    .cpu_EXMEM2_data(tb_EXMEM2_data),
    .cpu_EXMEM2_we(tb_EXMEM2_we),
    .cpu_EXMEM2_done(tb_EXMEM2_done),
    .cpu_EXMEM2_result(tb_EXMEM2_result),
    
    .cpu_clear_cache(tb_clear_cache),
    .cpu_clear_cache_done(tb_clear_cache_done),
    
    .l1i_ctrl_d(l1i_ctrl_d),
    .l1i_ctrl_addr(l1i_ctrl_addr),
    .l1i_ctrl_we(l1i_ctrl_we),
    .l1i_ctrl_q(l1i_ctrl_q),
    
    .l1d_ctrl_d(l1d_ctrl_d),
    .l1d_ctrl_addr(l1d_ctrl_addr),
    .l1d_ctrl_we(l1d_ctrl_we),
    .l1d_ctrl_q(l1d_ctrl_q),
    
    .sdc_addr(sdc_addr),
    .sdc_data(sdc_data),
    .sdc_we(sdc_we),
    .sdc_start(sdc_start),
    .sdc_done(sdc_done),
    .sdc_q(sdc_q)
);

// Dummy assignments for pipe interface
assign l1i_pipe_addr = 7'd0;
assign l1d_pipe_addr = 7'd0;

//==================================================================
// Latency Measurement Variables
//==================================================================
integer cycle_count_100mhz;
integer cycle_count_50mhz;
integer start_time_100mhz;
integer start_time_50mhz;
integer latency_100mhz;
integer latency_50mhz;

// Track state transitions in cache controller
reg [7:0] prev_cc_state;

// Scenario results storage
integer l1i_miss_latency_100 = 0;
integer l1i_miss_latency_50 = 0;
integer l1d_read_miss_clean_latency_100 = 0;
integer l1d_read_miss_clean_latency_50 = 0;
integer l1d_read_miss_dirty_latency_100 = 0;
integer l1d_read_miss_dirty_latency_50 = 0;
integer l1d_write_hit_latency_100 = 0;
integer l1d_write_hit_latency_50 = 0;
integer l1d_write_miss_clean_latency_100 = 0;
integer l1d_write_miss_clean_latency_50 = 0;
integer l1d_write_miss_dirty_latency_100 = 0;
integer l1d_write_miss_dirty_latency_50 = 0;

//==================================================================
// Clock Generation
//==================================================================
always #5 clk100 = ~clk100;   // 100 MHz
always #10 clk = ~clk;        // 50 MHz

// Cycle counters
always @(posedge clk100) begin
    if (reset)
        cycle_count_100mhz <= 0;
    else
        cycle_count_100mhz <= cycle_count_100mhz + 1;
end

always @(posedge clk) begin
    if (reset)
        cycle_count_50mhz <= 0;
    else
        cycle_count_50mhz <= cycle_count_50mhz + 1;
end

//==================================================================
// Test Tasks
//==================================================================

// Wait for SDRAM initialization
task wait_sdram_init;
begin
    // SDRAM needs ~100 cycles to initialize in simulation mode
    $display("Waiting for SDRAM initialization...");
    repeat(200) @(posedge clk100);
    $display("SDRAM initialization complete at cycle %0d", cycle_count_100mhz);
end
endtask

// Clear cache before tests
task clear_cache;
begin
    $display("\n--- Clearing cache ---");
    @(posedge clk);
    tb_clear_cache <= 1'b1;
    @(posedge clk);
    tb_clear_cache <= 1'b0;
    
    // Wait for clear to complete
    wait(tb_clear_cache_done);
    @(posedge clk100);
    $display("Cache cleared at cycle %0d (100MHz)", cycle_count_100mhz);
end
endtask

// Test L1I cache miss (instruction fetch)
task test_l1i_cache_miss;
    input [31:0] addr;
begin
    $display("\n=== TEST: L1I Cache Miss ===");
    $display("Address: 0x%08h", addr);
    
    // Record start time
    start_time_100mhz = cycle_count_100mhz;
    start_time_50mhz = cycle_count_50mhz;
    
    // Start FE2 request
    @(posedge clk);
    tb_FE2_addr <= addr;
    tb_FE2_start <= 1'b1;
    @(posedge clk);
    tb_FE2_start <= 1'b0;
    
    // Wait for done
    wait(tb_FE2_done);
    @(posedge clk100);
    
    // Calculate latency
    l1i_miss_latency_100 = cycle_count_100mhz - start_time_100mhz;
    l1i_miss_latency_50 = cycle_count_50mhz - start_time_50mhz;
    
    $display("L1I Miss Latency: %0d cycles @100MHz, %0d cycles @50MHz", 
             l1i_miss_latency_100, l1i_miss_latency_50);
    $display("Result: 0x%08h", tb_FE2_result);
end
endtask

// Test L1D cache read miss (clean line)
task test_l1d_read_miss_clean;
    input [31:0] addr;
begin
    $display("\n=== TEST: L1D Read Miss (Clean Line) ===");
    $display("Address: 0x%08h", addr);
    
    start_time_100mhz = cycle_count_100mhz;
    start_time_50mhz = cycle_count_50mhz;
    
    @(posedge clk);
    tb_EXMEM2_addr <= addr;
    tb_EXMEM2_we <= 1'b0;  // Read operation
    tb_EXMEM2_start <= 1'b1;
    @(posedge clk);
    tb_EXMEM2_start <= 1'b0;
    
    wait(tb_EXMEM2_done);
    @(posedge clk100);
    
    l1d_read_miss_clean_latency_100 = cycle_count_100mhz - start_time_100mhz;
    l1d_read_miss_clean_latency_50 = cycle_count_50mhz - start_time_50mhz;
    
    $display("L1D Read Miss (Clean) Latency: %0d cycles @100MHz, %0d cycles @50MHz",
             l1d_read_miss_clean_latency_100, l1d_read_miss_clean_latency_50);
    $display("Result: 0x%08h", tb_EXMEM2_result);
end
endtask

// Test L1D cache write hit
task test_l1d_write_hit;
    input [31:0] addr;
    input [31:0] data;
begin
    $display("\n=== TEST: L1D Write Hit ===");
    $display("Address: 0x%08h, Data: 0x%08h", addr, data);
    
    start_time_100mhz = cycle_count_100mhz;
    start_time_50mhz = cycle_count_50mhz;
    
    @(posedge clk);
    tb_EXMEM2_addr <= addr;
    tb_EXMEM2_data <= data;
    tb_EXMEM2_we <= 1'b1;  // Write operation
    tb_EXMEM2_start <= 1'b1;
    @(posedge clk);
    tb_EXMEM2_start <= 1'b0;
    
    wait(tb_EXMEM2_done);
    @(posedge clk100);
    
    l1d_write_hit_latency_100 = cycle_count_100mhz - start_time_100mhz;
    l1d_write_hit_latency_50 = cycle_count_50mhz - start_time_50mhz;
    
    $display("L1D Write Hit Latency: %0d cycles @100MHz, %0d cycles @50MHz",
             l1d_write_hit_latency_100, l1d_write_hit_latency_50);
end
endtask

// Test L1D cache write miss (clean line)
task test_l1d_write_miss_clean;
    input [31:0] addr;
    input [31:0] data;
begin
    $display("\n=== TEST: L1D Write Miss (Clean Line) ===");
    $display("Address: 0x%08h, Data: 0x%08h", addr, data);
    
    start_time_100mhz = cycle_count_100mhz;
    start_time_50mhz = cycle_count_50mhz;
    
    @(posedge clk);
    tb_EXMEM2_addr <= addr;
    tb_EXMEM2_data <= data;
    tb_EXMEM2_we <= 1'b1;
    tb_EXMEM2_start <= 1'b1;
    @(posedge clk);
    tb_EXMEM2_start <= 1'b0;
    
    wait(tb_EXMEM2_done);
    @(posedge clk100);
    
    l1d_write_miss_clean_latency_100 = cycle_count_100mhz - start_time_100mhz;
    l1d_write_miss_clean_latency_50 = cycle_count_50mhz - start_time_50mhz;
    
    $display("L1D Write Miss (Clean) Latency: %0d cycles @100MHz, %0d cycles @50MHz",
             l1d_write_miss_clean_latency_100, l1d_write_miss_clean_latency_50);
end
endtask

// Test L1D cache read miss with dirty line eviction
task test_l1d_read_miss_dirty;
    input [31:0] addr;  // Address that will cause eviction of dirty line
begin
    $display("\n=== TEST: L1D Read Miss (Dirty Line Eviction) ===");
    $display("Address: 0x%08h (will evict dirty line)", addr);
    
    start_time_100mhz = cycle_count_100mhz;
    start_time_50mhz = cycle_count_50mhz;
    
    @(posedge clk);
    tb_EXMEM2_addr <= addr;
    tb_EXMEM2_we <= 1'b0;
    tb_EXMEM2_start <= 1'b1;
    @(posedge clk);
    tb_EXMEM2_start <= 1'b0;
    
    wait(tb_EXMEM2_done);
    @(posedge clk100);
    
    l1d_read_miss_dirty_latency_100 = cycle_count_100mhz - start_time_100mhz;
    l1d_read_miss_dirty_latency_50 = cycle_count_50mhz - start_time_50mhz;
    
    $display("L1D Read Miss (Dirty) Latency: %0d cycles @100MHz, %0d cycles @50MHz",
             l1d_read_miss_dirty_latency_100, l1d_read_miss_dirty_latency_50);
    $display("Result: 0x%08h", tb_EXMEM2_result);
end
endtask

// Test L1D cache write miss with dirty line eviction
task test_l1d_write_miss_dirty;
    input [31:0] addr;
    input [31:0] data;
begin
    $display("\n=== TEST: L1D Write Miss (Dirty Line Eviction) ===");
    $display("Address: 0x%08h, Data: 0x%08h", addr, data);
    
    start_time_100mhz = cycle_count_100mhz;
    start_time_50mhz = cycle_count_50mhz;
    
    @(posedge clk);
    tb_EXMEM2_addr <= addr;
    tb_EXMEM2_data <= data;
    tb_EXMEM2_we <= 1'b1;
    tb_EXMEM2_start <= 1'b1;
    @(posedge clk);
    tb_EXMEM2_start <= 1'b0;
    
    wait(tb_EXMEM2_done);
    @(posedge clk100);
    
    l1d_write_miss_dirty_latency_100 = cycle_count_100mhz - start_time_100mhz;
    l1d_write_miss_dirty_latency_50 = cycle_count_50mhz - start_time_50mhz;
    
    $display("L1D Write Miss (Dirty) Latency: %0d cycles @100MHz, %0d cycles @50MHz",
             l1d_write_miss_dirty_latency_100, l1d_write_miss_dirty_latency_50);
end
endtask

//==================================================================
// Main Test Sequence
//==================================================================
initial begin
    $dumpfile("Hardware/FPGA/Verilog/Simulation/Output/latency_benchmark.vcd");
    $dumpvars;
    
    $display("\n");
    $display("============================================================");
    $display("   FPGC Memory Latency Benchmark");
    $display("============================================================");
    $display("CPU Clock: 50 MHz (20ns period)");
    $display("Cache/SDRAM Clock: 100 MHz (10ns period)");
    $display("");
    
    // Initial reset
    reset = 1'b1;
    repeat(10) @(posedge clk100);
    reset = 1'b0;
    
    // Wait for SDRAM to initialize
    wait_sdram_init();
    
    //------------------------------------------------------------------
    // Scenario 1: L1I cache miss (instruction fetch)
    //------------------------------------------------------------------
    clear_cache();
    test_l1i_cache_miss(32'h00000000);  // Fetch from address 0
    
    //------------------------------------------------------------------
    // Scenario 2: L1D read miss on clean cache line
    //------------------------------------------------------------------
    clear_cache();
    test_l1d_read_miss_clean(32'h00000100);  // Read from different address
    
    //------------------------------------------------------------------
    // Scenario 3: L1D write miss on clean cache line
    //------------------------------------------------------------------
    clear_cache();
    test_l1d_write_miss_clean(32'h00000200, 32'hDEADBEEF);
    
    //------------------------------------------------------------------
    // Scenario 4: L1D write hit (line is now in cache from scenario 3)
    // First we need to make sure the line is in cache
    //------------------------------------------------------------------
    // Line should still be in cache from previous write
    // Write to same cache line but different offset
    test_l1d_write_hit(32'h00000201, 32'hCAFEBABE);
    
    //------------------------------------------------------------------
    // Scenario 5: L1D read miss with dirty line eviction
    // Write to address 0x200, then read from 0x400 (same cache index, different tag)
    //------------------------------------------------------------------
    clear_cache();
    // First write to create dirty line at cache index
    test_l1d_write_miss_clean(32'h00000000, 32'h11111111);
    // Now read from address with same cache index but different tag
    // Address 0x400 (1024) has same cache index bits [9:3] as 0x000
    test_l1d_read_miss_dirty(32'h00000400);
    
    //------------------------------------------------------------------
    // Scenario 6: L1D write miss with dirty line eviction
    //------------------------------------------------------------------
    clear_cache();
    // First write to create dirty line
    test_l1d_write_miss_clean(32'h00000010, 32'h22222222);
    // Write to address with same cache index but different tag
    test_l1d_write_miss_dirty(32'h00000410, 32'h33333333);
    
    //------------------------------------------------------------------
    // Summary Report
    //------------------------------------------------------------------
    $display("\n");
    $display("============================================================");
    $display("   LATENCY BENCHMARK RESULTS SUMMARY");
    $display("============================================================");
    $display("");
    $display("| Scenario                          | 100MHz cycles | 50MHz cycles |");
    $display("|-----------------------------------|---------------|--------------|");
    $display("| L1I Miss                          | %13d | %12d |", l1i_miss_latency_100, l1i_miss_latency_50);
    $display("| L1D Read Miss (Clean)             | %13d | %12d |", l1d_read_miss_clean_latency_100, l1d_read_miss_clean_latency_50);
    $display("| L1D Read Miss (Dirty Evict)       | %13d | %12d |", l1d_read_miss_dirty_latency_100, l1d_read_miss_dirty_latency_50);
    $display("| L1D Write Hit                     | %13d | %12d |", l1d_write_hit_latency_100, l1d_write_hit_latency_50);
    $display("| L1D Write Miss (Clean)            | %13d | %12d |", l1d_write_miss_clean_latency_100, l1d_write_miss_clean_latency_50);
    $display("| L1D Write Miss (Dirty Evict)      | %13d | %12d |", l1d_write_miss_dirty_latency_100, l1d_write_miss_dirty_latency_50);
    $display("");
    $display("Note: 1 cycle @50MHz = 2 cycles @100MHz");
    $display("SDRAM burst read takes ~11 cycles @100MHz (CL2, 8-word burst)");
    $display("SDRAM burst write takes ~12 cycles @100MHz");
    $display("");
    $display("============================================================");
    $display("   END OF BENCHMARK");
    $display("============================================================");
    
    #100;
    $finish;
end

// Timeout watchdog
initial begin
    #500000;  // 500us timeout
    $display("ERROR: Simulation timed out!");
    $finish;
end

endmodule
