/*
 * Cache controller for L1 data cache
 * Connects to:
 * - DPRAM for L1 data cache
 * - CPU pipeline for commands
 * - MIG7 for memory interface
 */
module CacheControllerL1d
#(
    parameter ADDR_WIDTH = 29,
    parameter DATA_WIDTH = 256
)
(
    input wire clk,
    input wire reset,

    // CPU pipeline interface
    input wire cpu_start,
    input wire [31:0] cpu_addr,
    input wire [31:0] cpu_data,
    input wire cpu_we,
    output reg cpu_done = 1'b0,
    output reg [31:0] cpu_result = 32'd0,
    output reg cpu_ready = 1'b0,

    // L1 data cache DPRAM interface
    output reg [273:0]  l1d_ctrl_d = 274'b0,
    output reg [6:0]    l1d_ctrl_addr = 7'b0,
    output reg          l1d_ctrl_we = 1'b0,
    input wire  [273:0] l1d_ctrl_q,

    // MIG7 interface
    input wire                      init_calib_complete,
    output reg [ADDR_WIDTH-1:0]     app_addr = {ADDR_WIDTH{1'b0}},
    output reg [2:0]                app_cmd = 3'b000,
    output reg                      app_en = 1'b0,
    input wire                      app_rdy,
    
    output reg [DATA_WIDTH-1:0]     app_wdf_data = {DATA_WIDTH{1'b0}},
    output reg                      app_wdf_end = 1'b0,
    output reg [DATA_WIDTH/8-1:0]   app_wdf_mask = {(DATA_WIDTH/8){1'b0}},
    output reg                      app_wdf_wren = 1'b0,
    input wire                      app_wdf_rdy,
    
    input [DATA_WIDTH-1:0]          app_rd_data,
    input wire                      app_rd_data_end,
    input wire                      app_rd_data_valid
    
);


localparam
    STATE_IDLE = 3'd0;

reg [7:0] state = STATE_IDLE;

always @ (posedge clk)
begin
    if (reset)
    begin
        cpu_done <= 1'b0;
        cpu_ready <= 1'b0;
        state <= STATE_IDLE;
        app_addr <= {ADDR_WIDTH{1'b0}};
        app_cmd <= 3'b000;
        app_en <= 1'b0;
        app_wdf_data <= {DATA_WIDTH{1'b0}};
        app_wdf_end <= 1'b0;
        app_wdf_mask <= {(DATA_WIDTH/8){1'b0}};
        app_wdf_wren <= 1'b0;
        l1d_ctrl_d <= 274'b0;
        l1d_ctrl_addr <= 7'b0;
        l1d_ctrl_we <= 1'b0;
        cpu_result <= 32'b0;
    end
    else
    begin
        cpu_done <= 1'b0;
        l1d_ctrl_we <= 1'b0;
        cpu_ready <= 1'b0;
        
        case (state)
            STATE_IDLE:
                begin
                    cpu_ready <= 1'b1; // Indicate that the controller is ready for a new command
                    if (cpu_start && init_calib_complete)
                    begin
                        cpu_ready <= 1'b0;
                        
                    end
                end
            
                
            default:
                begin
                    state <= STATE_IDLE;
                end
        endcase
    end
end

endmodule
