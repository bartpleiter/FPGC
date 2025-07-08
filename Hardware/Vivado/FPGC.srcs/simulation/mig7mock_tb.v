/*
 * Testbench for the MIG 7 Mock.
 * Designed to be used with the Icarus Verilog simulator for simplicity
 */
`timescale 1ns / 1ps

`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/MIG7Mock.v"

module mig7mock_tb ();

// Parameters
parameter ADDR_WIDTH = 29;
parameter DATA_WIDTH = 256;
parameter MASK_WIDTH = 32;

// Clock and reset
reg sys_clk;
reg sys_rst;

// MIG interface signals
wire ui_clk;
wire ui_clk_sync_rst;
wire init_calib_complete;
wire [ADDR_WIDTH-1:0] app_addr;
wire [2:0] app_cmd;
wire app_en;
wire app_rdy;
wire [DATA_WIDTH-1:0] app_wdf_data;
wire app_wdf_end;
wire [MASK_WIDTH-1:0] app_wdf_mask;
wire app_wdf_wren;
wire app_wdf_rdy;
wire [DATA_WIDTH-1:0] app_rd_data;
wire app_rd_data_end;
wire app_rd_data_valid;

// Test signals
wire test_pass;
wire test_fail;
wire test_complete;

// Clock generation - 100MHz
initial begin
    sys_clk = 1'b0;
    forever #5 sys_clk = ~sys_clk; // 10ns period = 100MHz
end

// Reset generation
initial begin
    sys_rst = 1'b1;
    #100; // Hold reset for 100ns
    sys_rst = 1'b0;
end

// Test timeout
initial begin
    #10000; // 10us timeout
    if (!test_complete) begin
        $display("ERROR: Test timeout!");
        $finish;
    end
end

// Test completion monitoring
always @(posedge ui_clk) begin
    if (test_complete) begin
        if (test_pass) begin
            $display("=== TEST PASSED ===");
        end
        else if (test_fail) begin
            $display("=== TEST FAILED ===");
        end
        #100; // Wait a bit before finishing
        $finish;
    end
end

// MIG7Mock instance
MIG7Mock #(
    .ADDR_WIDTH(ADDR_WIDTH),
    .DATA_WIDTH(DATA_WIDTH),
    .MASK_WIDTH(MASK_WIDTH),
    .RAM_DEPTH(1024)
) mig_mock (
    .sys_clk_i(sys_clk),
    .sys_rst(sys_rst),
    .ui_clk(ui_clk),
    .ui_clk_sync_rst(ui_clk_sync_rst),
    .init_calib_complete(init_calib_complete),
    
    .app_addr(app_addr),
    .app_cmd(app_cmd),
    .app_en(app_en),
    .app_rdy(app_rdy),
    
    .app_wdf_data(app_wdf_data),
    .app_wdf_end(app_wdf_end),
    .app_wdf_mask(app_wdf_mask),
    .app_wdf_wren(app_wdf_wren),
    .app_wdf_rdy(app_wdf_rdy),
    
    .app_rd_data(app_rd_data),
    .app_rd_data_end(app_rd_data_end),
    .app_rd_data_valid(app_rd_data_valid),
    
    .app_sr_req(1'b0),
    .app_ref_req(1'b0),
    .app_zq_req(1'b0),
    .app_sr_active(),
    .app_ref_ack(),
    .app_zq_ack()
);

// Simple test instance
simple_test #(
    .ADDR_WIDTH(ADDR_WIDTH),
    .DATA_WIDTH(DATA_WIDTH)
) test_inst (
    .clk(ui_clk),
    .rst(ui_clk_sync_rst),
    .init_calib_complete(init_calib_complete),
    
    .app_addr(app_addr),
    .app_cmd(app_cmd),
    .app_en(app_en),
    .app_rdy(app_rdy),
    
    .app_wdf_data(app_wdf_data),
    .app_wdf_end(app_wdf_end),
    .app_wdf_mask(app_wdf_mask),
    .app_wdf_wren(app_wdf_wren),
    .app_wdf_rdy(app_wdf_rdy),
    
    .app_rd_data(app_rd_data),
    .app_rd_data_end(app_rd_data_end),
    .app_rd_data_valid(app_rd_data_valid),
    
    .test_pass(test_pass),
    .test_fail(test_fail),
    .test_complete(test_complete)
);

// Initial display
initial begin
    $dumpfile("Hardware/Vivado/FPGC.srcs/simulation/output/mig7.vcd");
    $dumpvars;
    $display("=== MIG7Mock Testbench Started ===");
end



endmodule

module simple_test
#(
    parameter ADDR_WIDTH = 29,
    parameter DATA_WIDTH = 256
)
(
    input                           clk,
    input                           rst,
    input                           init_calib_complete,
    
    // MIG interface
    output reg [ADDR_WIDTH-1:0]     app_addr,
    output reg [2:0]                app_cmd,
    output reg                      app_en,
    input                           app_rdy,
    
    output reg [DATA_WIDTH-1:0]     app_wdf_data,
    output reg                      app_wdf_end,
    output reg [DATA_WIDTH/8-1:0]   app_wdf_mask,
    output reg                      app_wdf_wren,
    input                           app_wdf_rdy,
    
    input [DATA_WIDTH-1:0]          app_rd_data,
    input                           app_rd_data_end,
    input                           app_rd_data_valid,
    
    // Test outputs
    output reg                      test_pass,
    output reg                      test_fail,
    output reg                      test_complete
);

// Test states
localparam IDLE = 3'b000;
localparam WRITE_CMD = 3'b001;
localparam WRITE_DATA = 3'b010;
localparam READ_CMD = 3'b011;
localparam READ_WAIT = 3'b100;
localparam COMPARE = 3'b101;
localparam DONE = 3'b110;

reg [2:0] state;
reg [DATA_WIDTH-1:0] test_data;
reg [ADDR_WIDTH-1:0] test_addr;

// Test pattern - alternating bits
wire [DATA_WIDTH-1:0] test_pattern = {32{8'hA5}}; // 0xA5A5A5A5... pattern

always @(posedge clk) begin
    if (rst) begin
        state <= IDLE;
        app_addr <= {ADDR_WIDTH{1'b0}};
        app_cmd <= 3'b000;
        app_en <= 1'b0;
        app_wdf_data <= {DATA_WIDTH{1'b0}};
        app_wdf_end <= 1'b0;
        app_wdf_mask <= {DATA_WIDTH/8{1'b0}}; // Enable all bytes
        app_wdf_wren <= 1'b0;
        test_data <= {DATA_WIDTH{1'b0}};
        test_addr <= 29'h0000100; // Test address (offset from 0)
        test_pass <= 1'b0;
        test_fail <= 1'b0;
        test_complete <= 1'b0;
    end
    else if (init_calib_complete) begin
        case (state)
            IDLE: begin
                if (app_rdy) begin
                    // Start write command
                    test_data <= test_pattern;
                    app_addr <= test_addr;
                    app_cmd <= 3'b000; // Write command
                    app_en <= 1'b1;
                    state <= WRITE_CMD;
                end
            end
            
            WRITE_CMD: begin
                if (app_rdy) begin
                    app_en <= 1'b0;
                    app_wdf_data <= test_data;
                    app_wdf_end <= 1'b1;
                    app_wdf_wren <= 1'b1;
                    state <= WRITE_DATA;
                end
            end
            
            WRITE_DATA: begin
                if (app_wdf_rdy) begin
                    app_wdf_wren <= 1'b0;
                    app_wdf_end <= 1'b0;
                    // Start read command
                    app_addr <= test_addr;
                    app_cmd <= 3'b001; // Read command
                    app_en <= 1'b1;
                    state <= READ_CMD;
                end
            end
            
            READ_CMD: begin
                if (app_rdy) begin
                    app_en <= 1'b0;
                    state <= READ_WAIT;
                end
            end
            
            READ_WAIT: begin
                if (app_rd_data_valid && app_rd_data_end) begin
                    state <= COMPARE;
                end
            end
            
            COMPARE: begin
                // Compare read data with written data
                if (app_rd_data == test_data) begin
                    test_pass <= 1'b1;
                    $display("TEST PASSED: Read data matches written data");
                    $display("Written: 0x%h", test_data);
                    $display("Read:    0x%h", app_rd_data);
                end
                else begin
                    test_fail <= 1'b1;
                    $display("TEST FAILED: Read data does not match written data");
                    $display("Written: 0x%h", test_data);
                    $display("Read:    0x%h", app_rd_data);
                end
                test_complete <= 1'b1;
                state <= DONE;
            end
            
            DONE: begin
                // Test complete - stay in this state
            end
            
            default: begin
                state <= IDLE;
            end
        endcase
    end
end

endmodule
