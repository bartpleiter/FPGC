/*
 * Cache controller for L1 instruction cache
 * Connects to:
 * - DPRAM for L1 instruction cache
 * - CPU pipeline for commands
 * - MIG7 for memory interface
 */
module CacheControllerL1i
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
    output reg cpu_done = 1'b0,
    output reg [31:0] cpu_result = 32'd0,
    output reg cpu_ready = 1'b0,

    // L1 instruction cache DPRAM interface
    output reg [273:0]  l1i_ctrl_d = 274'b0,
    output reg [6:0]    l1i_ctrl_addr = 7'b0,
    output reg          l1i_ctrl_we = 1'b0,
    input wire  [273:0] l1i_ctrl_q,

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
    STATE_IDLE = 3'd0,
    STATE_READ_CMD = 3'd1,
    STATE_READ_WAIT = 3'd2,
    STATE_WRITE_CACHE = 3'd3,
    STATE_WAIT_WRITE = 3'd4,
    STATE_WAIT_WRITE2 = 3'd5,
    STATE_DONE = 3'd6;

reg [7:0] state = STATE_IDLE;
reg [31:0] stored_cpu_addr;
reg [255:0] cache_line_data;
reg [15:0] cache_tag;
reg [2:0] word_offset;

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
        l1i_ctrl_d <= 274'b0;
        l1i_ctrl_addr <= 7'b0;
        l1i_ctrl_we <= 1'b0;
        cpu_result <= 32'b0;
        stored_cpu_addr <= 32'b0;
        cache_line_data <= 256'b0;
        cache_tag <= 16'b0;
        word_offset <= 3'b0;
    end
    else
    begin
        cpu_done <= 1'b0;
        l1i_ctrl_we <= 1'b0;
        cpu_ready <= 1'b0;
        
        case (state)
            STATE_IDLE:
                begin
                    cpu_ready <= 1'b1; // Indicate that the controller is ready for a new command
                    if (cpu_start && init_calib_complete)
                    begin
                        cpu_ready <= 1'b0;
                        // Store the CPU address and calculate derived values
                        stored_cpu_addr <= cpu_addr;
                        cache_tag <= cpu_addr[25:10];
                        word_offset <= cpu_addr[2:0]; // Word offset within cache line
                        
                        // Set up MIG7 read command
                        // Convert word address to cache line address (align to 256-bit boundary)
                        app_addr <= {4'd0, cpu_addr[31:3]}; // Align to 256 bits (8 words)
                        app_cmd <= 3'b001; // Read command
                        app_en <= 1'b1; // Assert enable already until app_rdy is high in the next state
                        
                        state <= STATE_READ_CMD;
                    end
                end
                
            STATE_READ_CMD:
                begin
                    if (app_rdy)
                    begin
                        app_en <= 1'b0;
                        state <= STATE_READ_WAIT;
                    end
                end
                
            STATE_READ_WAIT:
                begin
                    if (app_rd_data_valid && app_rd_data_end)
                    begin
                        // Store the cache line data
                        cache_line_data <= app_rd_data;
                        
                        // Extract the requested 32-bit word based on offset
                        case (word_offset)
                            3'd0: cpu_result <= app_rd_data[31:0];
                            3'd1: cpu_result <= app_rd_data[63:32];
                            3'd2: cpu_result <= app_rd_data[95:64];
                            3'd3: cpu_result <= app_rd_data[127:96];
                            3'd4: cpu_result <= app_rd_data[159:128];
                            3'd5: cpu_result <= app_rd_data[191:160];
                            3'd6: cpu_result <= app_rd_data[223:192];
                            3'd7: cpu_result <= app_rd_data[255:224];
                        endcase
                        
                        state <= STATE_WRITE_CACHE;
                    end
                end
                
            STATE_WRITE_CACHE:
                begin
                    // Write cache line to DPRAM
                    // Format: {256bit_data, 16bit_tag, 1'b1(valid), 1'b0(dirty)}
                    l1i_ctrl_d <= {cache_line_data, cache_tag, 1'b1, 1'b0};
                    l1i_ctrl_addr <= stored_cpu_addr[9:3]; // Cache index
                    l1i_ctrl_we <= 1'b1;
                    
                    
                    state <= STATE_WAIT_WRITE;
                end
                
            STATE_WAIT_WRITE:
                begin
                    // Wait until the data is written so that the fetch of the next instruction can use the cache
                    state <= STATE_WAIT_WRITE2;
                end

            STATE_WAIT_WRITE2:
                begin
                    // An extra state to really ensure the next instruction uses the newly written cache line
                    // TODO: optimize this state out if possible
                    state <= STATE_DONE;
                    cpu_done <= 1'b1;
                end
            
            STATE_DONE:
                begin
                    state <= STATE_IDLE;
                    cpu_ready <= 1'b1; // Indicate that the controller is ready for a new command
                end
                
            default:
                begin
                    state <= STATE_IDLE;
                end
        endcase
    end
end

endmodule
