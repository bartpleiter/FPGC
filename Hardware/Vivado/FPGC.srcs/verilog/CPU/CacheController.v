/*
 * Cache controller for L1 instruction and data cache
 * Runs at 100 MHz as it needs to connect to MIG7
 * - Aside from the CPU pipeline interface (50 MHz), everything else is also in the 100 MHz domain
 * Connects to:
 * - DPRAM for L1 instruction cache (hardcoded to 7 bits, or 128 lines)
 * - DPRAM for L1 data cache (hardcoded to 7 bits, or 128 lines)
 * - CPU pipeline for commands from FE2 and EXMEM2 stages
 * - MIG7 for memory interface
 */
module CacheController
#(
    parameter ADDR_WIDTH = 29,
    parameter DATA_WIDTH = 256,
    parameter MASK_WIDTH = 32
)
(
    //========================
    // System interface
    //========================
    input  wire                      clk100,
    input  wire                      reset,

    //========================
    // CPU pipeline interface (50 MHz domain)
    //========================
    // FE2 stage
    input  wire                      cpu_FE2_start,
    input  wire [31:0]               cpu_FE2_addr, // Address in CPU words for instruction fetch
    output reg                       cpu_FE2_done      = 1'b0,
    output reg [31:0]                cpu_FE2_result    = 32'd0, // Result of the instruction fetch
    // EXMEM2 stage
    input  wire                      cpu_EXMEM2_start,
    input  wire [31:0]               cpu_EXMEM2_addr, // Address in CPU words for data access
    input  wire [31:0]               cpu_EXMEM2_data,
    input  wire                      cpu_EXMEM2_we,
    output reg                       cpu_EXMEM2_done   = 1'b0,
    output reg [31:0]                cpu_EXMEM2_result = 32'd0, // Result of the data access

    //========================
    // L1 cache DPRAM interface
    //========================
    // L1i cache
    output reg  [273:0]              l1i_ctrl_d        = 274'b0,
    output reg  [6:0]                l1i_ctrl_addr     = 7'b0,
    output reg                       l1i_ctrl_we       = 1'b0,
    input  wire [273:0]              l1i_ctrl_q,
    // L1d cache
    output reg  [273:0]              l1d_ctrl_d        = 274'b0,
    output reg  [6:0]                l1d_ctrl_addr     = 7'b0,
    output reg                       l1d_ctrl_we       = 1'b0,
    input  wire [273:0]              l1d_ctrl_q,

    //========================
    // MIG7 interface
    //========================
    input  wire                      init_calib_complete,
    output reg  [ADDR_WIDTH-1:0]     app_addr          = {ADDR_WIDTH{1'b0}},
    output reg  [2:0]                app_cmd           = 3'b000,
    output reg                       app_en            = 1'b0,
    input  wire                      app_rdy,
    
    output reg  [DATA_WIDTH-1:0]     app_wdf_data      = {DATA_WIDTH{1'b0}},
    output reg                       app_wdf_end       = 1'b0,
    output reg  [MASK_WIDTH-1:0]     app_wdf_mask      = {MASK_WIDTH{1'b0}},
    output reg                       app_wdf_wren      = 1'b0,
    input  wire                      app_wdf_rdy,
    
    input  wire [DATA_WIDTH-1:0]     app_rd_data,
    input  wire                      app_rd_data_end,
    input  wire                      app_rd_data_valid
);

localparam
    STATE_IDLE = 3'd0,
    STATE_L1I_READ_CMD = 3'd1,
    STATE_L1I_READ_WAIT = 3'd2,
    STATE_L1I_WAIT_CACHE_WRITE = 3'd3,
    STATE_L1I_WAIT_CPU_DONE_50 = 3'd4;

reg [7:0] state = STATE_IDLE;


// Storing requests from CPU pipeline when they arrive, to process them when state machine is ready for new requests
reg cpu_FE2_new_request = 1'b0;
reg [31:0] cpu_FE2_addr_stored = 32'd0;
// For reference: cpu_FE2_cache_tag = cpu_FE2_addr_stored[25:10]
// For reference: cpu_FE2_cache_index = cpu_FE2_addr_stored[2:0]

reg cpu_EXMEM2_new_request = 1'b0;
reg [31:0] cpu_EXMEM2_addr_stored = 32'd0;
reg [31:0] cpu_EXMEM2_data_stored = 32'd0;
reg cpu_EXMEM2_we_stored = 1'b0;
// For reference: cpu_EXMEM2_cache_tag = cpu_EXMEM2_addr_stored[25:10]
// For reference: cpu_EXMEM2_cache_index = cpu_EXMEM2_addr_stored[2:0]

reg [255:0] cache_line_data = 256'b0;


always @ (posedge clk100)
begin
    if (reset)
    begin
        cpu_FE2_done <= 1'b0;
        cpu_FE2_result <= 32'd0;
        cpu_EXMEM2_done <= 1'b0;
        cpu_EXMEM2_result <= 32'd0;
        app_addr <= {ADDR_WIDTH{1'b0}};
        app_cmd <= 3'b000;
        app_en <= 1'b0;
        app_wdf_data <= {DATA_WIDTH{1'b0}};
        app_wdf_end <= 1'b0;
        app_wdf_mask <= {MASK_WIDTH{1'b0}};
        app_wdf_wren <= 1'b0;
        l1i_ctrl_d <= 274'b0;
        l1i_ctrl_addr <= 7'b0;
        l1i_ctrl_we <= 1'b0;
        l1d_ctrl_d <= 274'b0;
        l1d_ctrl_addr <= 7'b0;
        l1d_ctrl_we <= 1'b0;

        state <= STATE_IDLE;

        cpu_FE2_new_request <= 1'b0;
        cpu_FE2_addr_stored <= 32'd0;
        cpu_EXMEM2_new_request <= 1'b0;
        cpu_EXMEM2_addr_stored <= 32'd0;
        cpu_EXMEM2_data_stored <= 32'd0;
        cpu_EXMEM2_we_stored <= 1'b0;

        cache_line_data <= 256'b0;
    end
    else
    begin
        // Check for CPU FE2 request
        if (cpu_FE2_start)
        begin
            cpu_FE2_new_request <= 1'b1;
            cpu_FE2_addr_stored <= cpu_FE2_addr;
        end

        // Check for CPU EXMEM2 request
        if (cpu_EXMEM2_start)
        begin
            cpu_EXMEM2_new_request <= 1'b1;
            cpu_EXMEM2_addr_stored <= cpu_EXMEM2_addr;
            cpu_EXMEM2_data_stored <= cpu_EXMEM2_data;
            cpu_EXMEM2_we_stored <= cpu_EXMEM2_we;
        end

        // State machine to handle requests
        case (state)
            STATE_IDLE: begin
                // Disassert signals on idle
                cpu_FE2_done <= 1'b0;

                // Check if there is a new EXMEM2 request (priority over FE2)
                if ((cpu_EXMEM2_new_request || cpu_EXMEM2_start) && init_calib_complete) // Also check for start to skip a cycle after idle
                begin
                    cpu_EXMEM2_new_request <= 1'b0; // Clear the request flag

                    // TODO: Handle EXMEM2 requests
                end

                // Check if there is a new FE2 request (lower priority than EXMEM2)
                else if ((cpu_FE2_new_request || cpu_FE2_start) && init_calib_complete) // Also check for start to skip a cycle after idle
                begin
                    // FE2 requests are only READ operations
                    // Setup MIG7 read command with arguments depending on the availability in the _stored registers
                    app_cmd <= 3'b001; // READ command
                    app_en <= 1'b1;
                    app_addr <= cpu_FE2_start ? {4'd0, cpu_FE2_addr[31:3]} : {4'd0, cpu_FE2_addr_stored[31:3]}; // Align to 256 bits (8 words)

                    cpu_FE2_new_request <= 1'b0; // Clear the request flag

                    state <= STATE_L1I_READ_CMD; // Wait for MIG7 to accept the read command
                end
            end

            STATE_L1I_READ_CMD: begin
                // Wait for MIG7 to assert ready, indicating the read command is accepted
                // At this point the address is stored, so we should use it in case cpu does not set the signals anymore
                app_addr <= {4'd0, cpu_FE2_addr_stored[31:3]}; // Align to 256 bits (8 words)
                if (app_rdy)
                begin
                    // MIG7 is ready, we can proceed with the read operation
                    app_en <= 1'b0; // Disassert app_en to prevent sending another command
                    state <= STATE_L1I_READ_WAIT; // Wait until the data is ready
                end
            end

            STATE_L1I_READ_WAIT: begin
                // Wait for MIG7 to provide the read data
                if (app_rd_data_valid && app_rd_data_end)
                begin
                    // Extract the requested 32-bit word based on offset for the CPU return value
                    case (cpu_FE2_addr_stored[2:0])
                        3'd0: cpu_FE2_result <= app_rd_data[31:0];
                        3'd1: cpu_FE2_result <= app_rd_data[63:32];
                        3'd2: cpu_FE2_result <= app_rd_data[95:64];
                        3'd3: cpu_FE2_result <= app_rd_data[127:96];
                        3'd4: cpu_FE2_result <= app_rd_data[159:128];
                        3'd5: cpu_FE2_result <= app_rd_data[191:160];
                        3'd6: cpu_FE2_result <= app_rd_data[223:192];
                        3'd7: cpu_FE2_result <= app_rd_data[255:224];
                    endcase

                    // Write the retrieved cache line directly to DPRAM
                    // Format: {256bit_data, 16bit_tag, 1'b1(valid), 1'b0(dirty)}
                    l1i_ctrl_d <= {app_rd_data, cpu_FE2_addr_stored[25:10], 1'b1, 1'b0};
                    l1i_ctrl_addr <= cpu_FE2_addr_stored[9:3]; // DPRAM index, aligned on cache line size (8 words = 256 bits)
                    l1i_ctrl_we <= 1'b1;

                    state <= STATE_L1I_WAIT_CACHE_WRITE; // Wait until the data is written so that the fetch of the next instruction can use the cache
                end
            end

            STATE_L1I_WAIT_CACHE_WRITE: begin
                // Start by disasserting the write enable
                l1i_ctrl_we <= 1'b0;

                // Set cpu_done
                cpu_FE2_done <= 1'b1;
                state <= STATE_L1I_WAIT_CPU_DONE_50; // Extra stage for the 50 MHz CPU to see the results

            end

            STATE_L1I_WAIT_CPU_DONE_50: begin
                state <= STATE_IDLE; // After this stage, we can return to IDLE state
            end
        endcase
    end
end


endmodule
