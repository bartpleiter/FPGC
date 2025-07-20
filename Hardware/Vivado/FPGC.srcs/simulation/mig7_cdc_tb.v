/*
 * Testbench for MIG7 Clock Domain Crossing
 * Tests synchronous CDC between 50MHz and 100MHz domains
 */
`timescale 1ns / 1ps

`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/MIG7CDC.v"

module mig7_cdc_tb ();

// Clock and reset signals
reg clk_50 = 1'b0;
reg clk_100 = 1'b0;
reg reset_50 = 1'b0;
reg reset_100 = 1'b0;

// Arbiter side (50MHz domain) signals
reg [28:0] arb_app_addr;
reg [2:0] arb_app_cmd;
reg arb_app_en;
wire arb_app_rdy;

reg [255:0] arb_app_wdf_data;
reg arb_app_wdf_end;
reg [31:0] arb_app_wdf_mask;
reg arb_app_wdf_wren;
wire arb_app_wdf_rdy;

wire [255:0] arb_app_rd_data;
wire arb_app_rd_data_end;
wire arb_app_rd_data_valid;
wire arb_init_calib_complete;

// MIG7 side (100MHz domain) signals  
reg mig7_init_calib_complete;
wire [28:0] mig7_app_addr;
wire [2:0] mig7_app_cmd;
wire mig7_app_en;
reg mig7_app_rdy;

wire [255:0] mig7_app_wdf_data;
wire mig7_app_wdf_end;
wire [31:0] mig7_app_wdf_mask;
wire mig7_app_wdf_wren;
reg mig7_app_wdf_rdy;

reg [255:0] mig7_app_rd_data;
reg mig7_app_rd_data_end;
reg mig7_app_rd_data_valid;

// Instantiate the CDC module
MIG7CDC dut (
    .clk_50(clk_50),
    .reset_50(reset_50),
    .clk_100(clk_100),
    .reset_100(reset_100),
    
    // Arbiter side
    .arb_app_addr(arb_app_addr),
    .arb_app_cmd(arb_app_cmd),
    .arb_app_en(arb_app_en),
    .arb_app_rdy(arb_app_rdy),
    .arb_app_wdf_data(arb_app_wdf_data),
    .arb_app_wdf_end(arb_app_wdf_end),
    .arb_app_wdf_mask(arb_app_wdf_mask),
    .arb_app_wdf_wren(arb_app_wdf_wren),
    .arb_app_wdf_rdy(arb_app_wdf_rdy),
    .arb_app_rd_data(arb_app_rd_data),
    .arb_app_rd_data_end(arb_app_rd_data_end),
    .arb_app_rd_data_valid(arb_app_rd_data_valid),
    .arb_init_calib_complete(arb_init_calib_complete),
    
    // MIG7 side
    .mig7_init_calib_complete(mig7_init_calib_complete),
    .mig7_app_addr(mig7_app_addr),
    .mig7_app_cmd(mig7_app_cmd),
    .mig7_app_en(mig7_app_en),
    .mig7_app_rdy(mig7_app_rdy),
    .mig7_app_wdf_data(mig7_app_wdf_data),
    .mig7_app_wdf_end(mig7_app_wdf_end),
    .mig7_app_wdf_mask(mig7_app_wdf_mask),
    .mig7_app_wdf_wren(mig7_app_wdf_wren),
    .mig7_app_wdf_rdy(mig7_app_wdf_rdy),
    .mig7_app_rd_data(mig7_app_rd_data),
    .mig7_app_rd_data_end(mig7_app_rd_data_end),
    .mig7_app_rd_data_valid(mig7_app_rd_data_valid)
);

// Clock generation - 100MHz clock
always begin
    #5 clk_100 = ~clk_100; // 10ns period = 100MHz
end

// Clock generation - 50MHz clock (exactly half of 100MHz, in sync)
always begin
    #10 clk_50 = ~clk_50; // 20ns period = 50MHz
end

// Test variables
integer test_count = 0;
integer cycle_count = 0;

// Monitor for debugging
initial begin
    $monitor("Time=%0t clk_50=%b clk_100=%b phase=%b arb_en=%b mig7_en=%b arb_rdy=%b mig7_rdy=%b", 
             $time, clk_50, clk_100, dut.clk_100_phase, arb_app_en, mig7_app_en, arb_app_rdy, mig7_app_rdy);
end

// Test sequence
initial begin
    $display("Starting MIG7 CDC testbench");
    
    // Initialize signals
    arb_app_addr = 29'h0;
    arb_app_cmd = 3'b0;
    arb_app_en = 1'b0;
    arb_app_wdf_data = 256'h0;
    arb_app_wdf_end = 1'b0;
    arb_app_wdf_mask = 32'h0;
    arb_app_wdf_wren = 1'b0;
    
    mig7_init_calib_complete = 1'b0;
    mig7_app_rdy = 1'b0;
    mig7_app_wdf_rdy = 1'b0;
    mig7_app_rd_data = 256'h0;
    mig7_app_rd_data_end = 1'b0;
    mig7_app_rd_data_valid = 1'b0;
    
    // Reset sequence
    reset_50 = 1'b1;
    reset_100 = 1'b1;
    #50;
    reset_50 = 1'b0;
    reset_100 = 1'b0;
    #50;
    
    $display("Reset complete, starting tests");
    
    // Test 1: Init calibration complete crossing
    $display("\n=== Test 1: Init calibration complete crossing ===");
    mig7_init_calib_complete = 1'b1;
    #40; // Wait 2 full 50MHz cycles
    if (arb_init_calib_complete !== 1'b1) begin
        $display("ERROR: Init calib complete not crossed correctly");
        $finish;
    end
    $display("PASS: Init calib complete crossed successfully");
    
    // Test 2: Command crossing (50MHz -> 100MHz)
    $display("\n=== Test 2: Command crossing ===");
    mig7_app_rdy = 1'b1;
    @(posedge clk_50);
    arb_app_addr = 29'h1234567;
    arb_app_cmd = 3'b001; // Read command
    arb_app_en = 1'b1;
    
    // Check that signals appear on MIG7 side within 1 100MHz cycle after the next 100MHz edge
    @(posedge clk_100);
    @(posedge clk_100);  // Wait one more 100MHz cycle to allow for sampling
    if (mig7_app_addr !== 29'h1234567 || mig7_app_cmd !== 3'b001 || mig7_app_en !== 1'b1) begin
        $display("ERROR: Command signals not crossed correctly");
        $display("  Expected: addr=0x%h cmd=%b en=%b", 29'h1234567, 3'b001, 1'b1);
        $display("  Got:      addr=0x%h cmd=%b en=%b", mig7_app_addr, mig7_app_cmd, mig7_app_en);
        $finish;
    end
    $display("PASS: Command signals crossed successfully");
    
    // Test 3: Ready signal crossing (100MHz -> 50MHz)
    $display("\n=== Test 3: Ready signal crossing ===");
    // Ready should appear on arbiter side
    @(posedge clk_50);
    if (arb_app_rdy !== 1'b1) begin
        $display("ERROR: Ready signal not crossed correctly");
        $finish;
    end
    $display("PASS: Ready signal crossed successfully");
    
    // Test 4: Write data crossing
    $display("\n=== Test 4: Write data crossing ===");
    arb_app_en = 1'b0; // End previous command
    @(posedge clk_50);
    
    mig7_app_wdf_rdy = 1'b1;
    arb_app_cmd = 3'b000; // Write command
    arb_app_en = 1'b1;
    arb_app_wdf_data = 256'hDEADBEEFCAFEBABE_1234567890ABCDEF_FEDCBA0987654321_ABCDEF1234567890;
    arb_app_wdf_end = 1'b1;
    arb_app_wdf_mask = 32'h00000000; // No mask
    arb_app_wdf_wren = 1'b1;
    
    @(posedge clk_100);
    @(posedge clk_100);  // Wait for sampling
    if (mig7_app_wdf_data !== arb_app_wdf_data || mig7_app_wdf_end !== 1'b1 || 
        mig7_app_wdf_mask !== 32'h00000000 || mig7_app_wdf_wren !== 1'b1) begin
        $display("ERROR: Write data signals not crossed correctly");
        $finish;
    end
    $display("PASS: Write data signals crossed successfully");
    
    // Test 5: Read data crossing (100MHz -> 50MHz) 
    $display("\n=== Test 5: Read data crossing ===");
    arb_app_en = 1'b0;
    arb_app_wdf_wren = 1'b0;
    @(posedge clk_50);
    
    // Simulate MIG7 providing read data
    @(posedge clk_100);
    mig7_app_rd_data = 256'h1111222233334444_5555666677778888_9999AAAABBBBCCCC_DDDDEEEEFFFFABCD;
    mig7_app_rd_data_end = 1'b1;
    mig7_app_rd_data_valid = 1'b1;
    
    @(posedge clk_100);
    mig7_app_rd_data_valid = 1'b0; // End the pulse
    
    // Check if data appears on arbiter side
    @(posedge clk_50);
    if (arb_app_rd_data !== mig7_app_rd_data || arb_app_rd_data_end !== 1'b1) begin
        $display("ERROR: Read data not crossed correctly");
        $display("  Expected: data=0x%h end=%b", mig7_app_rd_data, 1'b1);
        $display("  Got:      data=0x%h end=%b", arb_app_rd_data, arb_app_rd_data_end);
        $finish;
    end
    
    // Check that valid pulse appears correctly
    if (arb_app_rd_data_valid !== 1'b1) begin
        $display("ERROR: Read data valid pulse not generated correctly");
        $finish;
    end
    
    @(posedge clk_50);
    if (arb_app_rd_data_valid !== 1'b0) begin
        $display("ERROR: Read data valid pulse too long");
        $finish;
    end
    $display("PASS: Read data crossed successfully with correct pulse timing");
    
    // Test 6: Phase relationship verification
    $display("\n=== Test 6: Phase relationship verification ===");
    // Verify that phase signal correctly tracks the relationship between clocks
    @(posedge clk_50);
    #1; // Small delay to check phase after 50MHz edge
    if (dut.clk_100_phase !== 1'b0) begin
        $display("ERROR: Phase signal incorrect after 50MHz rising edge");
        $finish;
    end
    
    // Wait for next 100MHz edge (should be phase 1)
    wait (clk_100 == 1'b1);
    #1;
    if (dut.clk_100_phase !== 1'b1) begin
        $display("ERROR: Phase signal should be 1 on 100MHz high phase");
        $finish;
    end
    $display("PASS: Phase relationship working correctly");
    
    $display("\n=== All tests passed! ===");
    $display("MIG7 CDC is working correctly");
    
    #100;
    $finish;
end

// Safety timeout
initial begin
    #10000;
    $display("ERROR: Testbench timeout");
    $finish;
end

// VCD dump for waveform analysis
initial begin
    $dumpfile("Hardware/Vivado/FPGC.srcs/simulation/output/mig7_cdc.vcd");
    $dumpvars(0, mig7_cdc_tb);
end

endmodule
