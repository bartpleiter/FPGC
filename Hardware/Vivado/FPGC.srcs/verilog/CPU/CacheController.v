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
    parameter ADDR_WIDTH = 29, // -> I actually think this should be 23 bits, as we have 23 bit line addresses, although this is for 256MiB
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
    input  wire                      cpu_FE2_flush, // CPU is flushed, do not set the done signal when the fetch completes
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
    STATE_IDLE = 8'd0,

    // L1 Instruction Cache States
    STATE_L1I_SEND_READ_CMD      = 8'd1,
    STATE_L1I_WAIT_READ_DATA     = 8'd2,
    STATE_L1I_WRITE_TO_CACHE     = 8'd3,
    STATE_L1I_SIGNAL_CPU_DONE    = 8'd4,

    // L1 Data Cache Read States
    STATE_L1D_READ_WAIT_CACHE_READ       = 8'd5,
    STATE_L1D_READ_CHECK_CACHE           = 8'd6,
    STATE_L1D_READ_EVICT_DIRTY_WAIT_READY= 8'd7,
    STATE_L1D_READ_EVICT_DIRTY_SEND_CMD  = 8'd8,
    STATE_L1D_READ_SEND_CMD              = 8'd9,
    STATE_L1D_READ_WAIT_READY            = 8'd10,
    STATE_L1D_READ_WAIT_DATA             = 8'd11,
    STATE_L1D_READ_WRITE_TO_CACHE        = 8'd12,
    STATE_L1D_READ_SIGNAL_CPU_DONE       = 8'd13,

    // L1 Data Cache Write States
    STATE_L1D_WRITE_WAIT_CACHE_READ          = 8'd14,
    STATE_L1D_WRITE_CHECK_CACHE              = 8'd15,
    STATE_L1D_WRITE_MISS_EVICT_DIRTY_WAIT_READY = 8'd16,
    STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD   = 8'd17,
    STATE_L1D_WRITE_MISS_FETCH_SEND_CMD         = 8'd18,
    STATE_L1D_WRITE_MISS_FETCH_WAIT_READY       = 8'd19,
    STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA        = 8'd20,
    STATE_L1D_WRITE_WRITE_TO_CACHE              = 8'd21,
    STATE_L1D_WRITE_SIGNAL_CPU_DONE             = 8'd22;


reg [7:0] state = STATE_IDLE;


// Storing requests from CPU pipeline when they arrive, to process them when state machine is ready for new requests
reg cpu_FE2_start_prev = 1'b0; // For edge detection as the start signal will be at 50 MHz
reg cpu_FE2_new_request = 1'b0;
reg [31:0] cpu_FE2_addr_stored = 32'd0;
// For reference: cpu_FE2_cache_tag = cpu_FE2_addr_stored[25:10]
// For reference: cpu_FE2_cache_index = cpu_FE2_addr_stored[2:0]

reg cpu_EXMEM2_start_prev = 1'b0; // For edge detection as the start signal will be at 50 MHz
reg cpu_EXMEM2_new_request = 1'b0;
reg [31:0] cpu_EXMEM2_addr_stored = 32'd0;
reg [31:0] cpu_EXMEM2_data_stored = 32'd0;
reg cpu_EXMEM2_we_stored = 1'b0;
// For reference: cpu_EXMEM2_cache_tag = cpu_EXMEM2_addr_stored[25:10]
// For reference: cpu_EXMEM2_cache_index = cpu_EXMEM2_addr_stored[2:0]

reg [255:0] cache_line_data = 256'b0;

reg ignore_fe2_result = 1'b0; // If a flush is received while processing a FE2 request, we need to ignore the result when it is done

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

        cpu_FE2_start_prev <= 1'b0;
        cpu_FE2_new_request <= 1'b0;
        cpu_FE2_addr_stored <= 32'd0;
        cpu_EXMEM2_start_prev <= 1'b0;
        cpu_EXMEM2_new_request <= 1'b0;
        cpu_EXMEM2_addr_stored <= 32'd0;
        cpu_EXMEM2_data_stored <= 32'd0;
        cpu_EXMEM2_we_stored <= 1'b0;

        cache_line_data <= 256'b0;

        ignore_fe2_result <= 1'b0;
    end
    else
    begin
        // Edge detection on CPU start signals
        cpu_FE2_start_prev <= cpu_FE2_start;
        cpu_EXMEM2_start_prev <= cpu_EXMEM2_start;

        // Check for CPU FE2 request
        if (cpu_FE2_start && !cpu_FE2_start_prev)
        begin
            cpu_FE2_new_request <= 1'b1;
            cpu_FE2_addr_stored <= cpu_FE2_addr;
            $display("%d: CacheController NEW FE2 REQUEST: addr=0x%h", $time, cpu_FE2_addr);
        end

        // Check for CPU EXMEM2 request
        if (cpu_EXMEM2_start && !cpu_EXMEM2_start_prev)
        begin
            cpu_EXMEM2_new_request <= 1'b1;
            cpu_EXMEM2_addr_stored <= cpu_EXMEM2_addr;
            cpu_EXMEM2_data_stored <= cpu_EXMEM2_data;
            cpu_EXMEM2_we_stored <= cpu_EXMEM2_we;
            $display("%d: CacheController NEW EXMEM2 REQUEST: addr=0x%h, data=0x%h, we=%b", $time, cpu_EXMEM2_addr, cpu_EXMEM2_data, cpu_EXMEM2_we);
        end

        // Flush handling for FE2
        if (cpu_FE2_flush)
        begin
            cpu_FE2_new_request <= 1'b0; // Clear any pending request

            // If state machine is in the middle of processing a FE2 request, we need to set a flag to ignore the result when it is done
            if (state == STATE_L1I_SEND_READ_CMD ||
                state == STATE_L1I_WAIT_READ_DATA)
            begin
                ignore_fe2_result <= 1'b1;
                $display("%d: CacheController FE2 FLUSH RECEIVED, will ignore result when done", $time);
            end
        end

        // State machine to handle requests
        case (state)
            STATE_IDLE: begin
                // Disassert signals on idle
                cpu_FE2_done <= 1'b0;
                cpu_EXMEM2_done <= 1'b0;
                

                // Check if there is a new EXMEM2 request (priority over FE2)
                if ((cpu_EXMEM2_new_request || cpu_EXMEM2_start) && init_calib_complete) // Also check for start to skip a cycle after idle
                begin
                    // Request can be either a read after cache miss, or a write which can be either a hit or a miss
                    cpu_EXMEM2_new_request <= 1'b0; // Clear the request flag
                    $display("%d: CacheController PROCESSING EXMEM2 REQUEST: addr=0x%h, we=%b", $time, cpu_EXMEM2_start ? cpu_EXMEM2_addr : cpu_EXMEM2_addr_stored, cpu_EXMEM2_start ? cpu_EXMEM2_we : cpu_EXMEM2_we_stored);

                    // Handle write request
                    if ((cpu_EXMEM2_new_request && cpu_EXMEM2_we_stored) || (cpu_EXMEM2_start && cpu_EXMEM2_we))
                    begin
                        $display("%d: CacheController EXMEM2 WRITE REQUEST", $time);
                        // Read cache line first to determine a hit or miss
                        l1d_ctrl_addr <= cpu_EXMEM2_start ? cpu_EXMEM2_addr[9:3] : cpu_EXMEM2_addr_stored[9:3]; // DPRAM index, aligned on cache line size (8 words = 256 bits)
                        l1d_ctrl_we <= 1'b0; // Read operation
                        state <= STATE_L1D_WRITE_WAIT_CACHE_READ; // Wait for DPRAM read to complete
                        $display("%d: CacheController IDLE -> STATE_L1D_WRITE_WAIT_CACHE_READ", $time);
                    end

                    // Handle read request
                    else if ((cpu_EXMEM2_new_request && !cpu_EXMEM2_we_stored) || (cpu_EXMEM2_start && !cpu_EXMEM2_we))
                    begin
                        $display("%d: CacheController EXMEM2 READ REQUEST", $time);
                        // Read cache line first to determine if it needs to be eviced to memory
                        l1d_ctrl_addr <= cpu_EXMEM2_start ? cpu_EXMEM2_addr[9:3] : cpu_EXMEM2_addr_stored[9:3]; // DPRAM index, aligned on cache line size (8 words = 256 bits)
                        l1d_ctrl_we <= 1'b0; // Read operation
                        state <= STATE_L1D_READ_WAIT_CACHE_READ; // Wait for DPRAM read to complete
                        $display("%d: CacheController IDLE -> STATE_L1D_READ_WAIT_CACHE_READ", $time);
                    end
                end

                // Check if there is a new FE2 request (lower priority than EXMEM2)
                else if ((cpu_FE2_new_request || cpu_FE2_start) && init_calib_complete) // Also check for start to skip a cycle after idle
                begin
                    $display("%d: CacheController PROCESSING FE2 REQUEST: addr=0x%h", $time, cpu_FE2_start ? cpu_FE2_addr : cpu_FE2_addr_stored);

                    // FE2 requests are only READ operations
                    // Setup MIG7 read command with arguments depending on the availability in the _stored registers
                    app_cmd <= 3'b001; // READ command
                    app_en <= 1'b1;
                    app_addr <= cpu_FE2_start ? {4'd0, cpu_FE2_addr[31:3]} : {4'd0, cpu_FE2_addr_stored[31:3]}; // Align to 256 bits (8 words)
                    $display("%d: CacheController MIG7 FE2 READ CMD: addr=0x%h", $time, cpu_FE2_start ? {4'd0, cpu_FE2_addr[31:3]} : {4'd0, cpu_FE2_addr_stored[31:3]});

                    cpu_FE2_new_request <= 1'b0; // Clear the request flag

                    state <= STATE_L1I_SEND_READ_CMD; // Wait for MIG7 to accept the read command
                    $display("%d: CacheController IDLE -> STATE_L1I_SEND_READ_CMD", $time);

                end
            end

            // ------------------------
            // L1 Instruction Cache Read States
            // ------------------------

            STATE_L1I_SEND_READ_CMD: begin
                // Wait for MIG7 to assert ready, indicating the read command is accepted
                // At this point the address is stored, so we should use it in case cpu does not set the signals anymore
                app_addr <= {4'd0, cpu_FE2_addr_stored[31:3]}; // Align to 256 bits (8 words)
                if (app_rdy)
                begin
                    // MIG7 is ready, we can proceed with the read operation
                    app_en <= 1'b0; // Disassert app_en to prevent sending another command
                    state <= STATE_L1I_WAIT_READ_DATA; // Wait until the data is ready
                end
            end

            STATE_L1I_WAIT_READ_DATA: begin
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

                    state <= STATE_L1I_WRITE_TO_CACHE; // Wait until the data is written so that the fetch of the next instruction can use the cache
                end
            end

            STATE_L1I_WRITE_TO_CACHE: begin
                // Start by disasserting the write enable
                l1i_ctrl_we <= 1'b0;

                // Set cpu_done
                if (!ignore_fe2_result && !cpu_FE2_flush)
                begin
                    cpu_FE2_done <= 1'b1;
                    $display("%d: CacheController FE2 OPERATION COMPLETE: addr=0x%h, result=0x%h", $time, cpu_FE2_addr_stored, cpu_FE2_result);
                end
                else
                begin
                    $display("%d: CacheController FE2 OPERATION COMPLETE: addr=0x%h, result=0x%h -- BUT IGNORED DUE TO FLUSH", $time, cpu_FE2_addr_stored, cpu_FE2_result);
                    ignore_fe2_result <= 1'b0; // Clear the ignore flag
                end
                state <= STATE_L1I_SIGNAL_CPU_DONE; // Extra stage for the 50 MHz CPU to see the results
            end

            STATE_L1I_SIGNAL_CPU_DONE: begin
                state <= STATE_IDLE;
            end

            // ------------------------
            // L1 Data Cache Read States
            // ------------------------


            STATE_L1D_READ_WAIT_CACHE_READ: begin
                // Wait one cycle for DPRAM read to complete
                state <= STATE_L1D_READ_CHECK_CACHE;
            end

            STATE_L1D_READ_CHECK_CACHE: begin
                // Cache line is in l1d_ctrl_q
                // valid = l1d_ctrl_q[1]
                // dirty = l1d_ctrl_q[0]
                // tag = l1d_ctrl_q[17:2]
                // data = l1d_ctrl_q[273:18]

                // Store the l1d_ctrl_q data in case it is needed in the next state
                cache_line_data <= l1d_ctrl_q[273:18]; // Store the cache line data

                // We already know it is a cache miss, otherwise the CPU would not have requested the read, so we only need to check for the dirty bit
                $display("%d: CacheController L1D READ cache miss: addr=0x%h, cache_line_valid=%b, cache_line_dirty=%b, cache_line_tag=0x%h", $time, cpu_EXMEM2_addr_stored, l1d_ctrl_q[1], l1d_ctrl_q[0], l1d_ctrl_q[17:2]);
                if (l1d_ctrl_q[0])
                begin
                    $display("%d: CacheController L1D cache line is dirty, need to evict first", $time);
                    // If dirty, we need to write to MIG7 before reading the new cache line
                    if (app_rdy && app_wdf_rdy)
                    begin
                        app_cmd <= 3'b000; // WRITE command
                        app_en <= 1'b1;
                        // We need to write the address of the old cache line, so we need to use:
                        // the tag l1d_ctrl_q[17:2]
                        // the index of the cache line cpu_EXMEM2_addr_stored[9:3], which is aligned to 256 bits (8 words)
                        app_addr <= {l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[9:3]}; // Align to 256 bits (8 words)
                        app_wdf_wren <= 1'b1; // Enable write data
                        app_wdf_data <= l1d_ctrl_q[273:18]; // Data to write, which is the cache line data
                        app_wdf_end <= 1'b1; // End of write data
                        $display("%d: CacheController MIG7 L1D EVICT WRITE CMD: addr=0x%h, data=0x%h", $time, {l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[9:3]}, l1d_ctrl_q[273:18]);

                        state <= STATE_L1D_READ_EVICT_DIRTY_SEND_CMD; // Wait for MIG7 to accept the write command
                    end
                    else
                    begin
                        // Go to state where we wait for MIG7 to be ready
                        state <= STATE_L1D_READ_EVICT_DIRTY_WAIT_READY;
                    end
                end
                else
                begin
                    $display("%d: CacheController L1D cache line is clean, can read new line directly", $time);
                    // If not dirty, we can directly read the new cache line
                    app_cmd <= 3'b001; // READ command
                    app_en <= 1'b1;
                    app_addr <= cpu_EXMEM2_addr_stored[31:3]; // Align to 256 bits (8 words)
                    $display("%d: CacheController MIG7 L1D READ CMD: addr=0x%h", $time, cpu_EXMEM2_addr_stored[31:3]);

                    state <= STATE_L1D_READ_WAIT_READY; // Wait for MIG7 to accept the read command
                end
            end

            STATE_L1D_READ_EVICT_DIRTY_WAIT_READY: begin
                // Wait for the MIG7 to be ready, so we can proceed with the write operation
                if (app_rdy && app_wdf_rdy)
                begin
                    app_cmd <= 3'b000; // WRITE command
                    app_en <= 1'b1;
                    // We need to write the address of the old cache line, so we need to use:
                    // the tag l1d_ctrl_q[17:2]
                    // the index of the cache line cpu_EXMEM2_addr_stored[9:3], which is aligned to 256 bits (8 words)
                    app_addr <= {l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[9:3]}; // Align to 256 bits (8 words) 
                    app_wdf_wren <= 1'b1; // Enable write data
                    app_wdf_data <= cache_line_data; // Data to write, which is the cache line data
                    app_wdf_end <= 1'b1; // End of write data
                    state <= STATE_L1D_READ_EVICT_DIRTY_SEND_CMD; // Wait for MIG7 to accept the write command
                end
            end

            STATE_L1D_READ_EVICT_DIRTY_SEND_CMD: begin
                // No matter if app_rdy, we can disassert app_wdf signals as it was ready the previous cycle, and I do not expect it to change
                app_wdf_wren <= 1'b0; // Disable write data
                app_wdf_end <= 1'b0; // End of write data
                // Wait for MIG7 to assert ready, indicating the write command is accepted
                if (app_rdy)
                begin
                    // MIG7 is ready, we can proceed with the read operation
                    app_en <= 1'b0; // Disassert app_en to prevent sending another command
                    state <= STATE_L1D_READ_SEND_CMD; // The cache line is now written to MIG7, we can start a read the next cycle
                end
            end

            STATE_L1D_READ_SEND_CMD: begin
                // Set the MIG7 read command for the new cache line
                app_cmd <= 3'b001; // READ command
                app_en <= 1'b1;
                app_addr <= cpu_EXMEM2_addr_stored[31:3]; // Align to 256 bits (8 words)
                $display("%d: CacheController MIG7 L1D READ CMD after evict: addr=0x%h", $time, cpu_EXMEM2_addr_stored[31:3]);

                state <= STATE_L1D_READ_WAIT_READY; // Wait for MIG7 to accept the read command
            end

            STATE_L1D_READ_WAIT_READY: begin
                // Wait for MIG7 to assert ready, indicating the read command is accepted
                if (app_rdy)
                begin
                    // MIG7 is ready, we can proceed with the read operation
                    app_en <= 1'b0; // Disassert app_en to prevent sending another command
                    state <= STATE_L1D_READ_WAIT_DATA; // Wait until the data is ready
                end
            end

            STATE_L1D_READ_WAIT_DATA: begin
                // Wait until data is ready, and write it to DPRAM of L1D cache
                if (app_rd_data_valid && app_rd_data_end)
                begin
                    // Extract the requested 32-bit word based on offset for the CPU return value
                    case (cpu_EXMEM2_addr_stored[2:0])
                        3'd0: cpu_EXMEM2_result <= app_rd_data[31:0];
                        3'd1: cpu_EXMEM2_result <= app_rd_data[63:32];
                        3'd2: cpu_EXMEM2_result <= app_rd_data[95:64];
                        3'd3: cpu_EXMEM2_result <= app_rd_data[127:96];
                        3'd4: cpu_EXMEM2_result <= app_rd_data[159:128];
                        3'd5: cpu_EXMEM2_result <= app_rd_data[191:160];
                        3'd6: cpu_EXMEM2_result <= app_rd_data[223:192];
                        3'd7: cpu_EXMEM2_result <= app_rd_data[255:224];
                    endcase

                    // Write the retrieved cache line directly to DPRAM
                    // Format: {256bit_data, 16bit_tag, 1'b1(valid), 1'b0(dirty)}
                    l1d_ctrl_d <= {app_rd_data, cpu_EXMEM2_addr_stored[25:10], 1'b1, 1'b0};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[9:3]; // DPRAM index, aligned on cache line size (8 words = 256 bits)
                    l1d_ctrl_we <= 1'b1;

                    state <= STATE_L1D_READ_WRITE_TO_CACHE; // Wait until the data is written so that the fetch of the next instruction can use the cache
                end
            end

            STATE_L1D_READ_WRITE_TO_CACHE: begin
                // Start by disasserting the write enable
                l1d_ctrl_we <= 1'b0;

                // Set cpu_done
                cpu_EXMEM2_done <= 1'b1;
                $display("%d: CacheController EXMEM2 READ OPERATION COMPLETE: addr=0x%h, result=0x%h", $time, cpu_EXMEM2_addr_stored, cpu_EXMEM2_result);
                state <= STATE_L1D_READ_SIGNAL_CPU_DONE; // Extra stage for the 50 MHz CPU to see the results

            end

            STATE_L1D_READ_SIGNAL_CPU_DONE: begin
                state <= STATE_IDLE; // After this stage, we can return to IDLE state
            end

            // ------------------------
            // L1 Data Cache Write States
            // ------------------------

            STATE_L1D_WRITE_WAIT_CACHE_READ: begin
                // Wait one cycle for DPRAM read to complete
                state <= STATE_L1D_WRITE_CHECK_CACHE;
            end

            STATE_L1D_WRITE_CHECK_CACHE: begin
                // Cache line is in l1d_ctrl_q
                // valid = l1d_ctrl_q[1]
                // dirty = l1d_ctrl_q[0]
                // tag = l1d_ctrl_q[17:2]
                // data = l1d_ctrl_q[273:18]

                // Store the l1d_ctrl_q data in case it is needed in the next state
                cache_line_data <= l1d_ctrl_q[273:18]; // Store the cache line data

                // Check for cache hit: valid bit is set and tag matches
                $display("%d: CacheController L1D WRITE check cache: addr=0x%h, cache_line_valid=%b, cache_line_dirty=%b, cache_line_tag=0x%h, expected_tag=0x%h", $time, cpu_EXMEM2_addr_stored, l1d_ctrl_q[1], l1d_ctrl_q[0], l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[25:10]);
                if (l1d_ctrl_q[1] && (l1d_ctrl_q[17:2] == cpu_EXMEM2_addr_stored[25:10]))
                begin
                    $display("%d: CacheController L1D WRITE CACHE HIT", $time);
                    // Cache hit: immediately write back to DPRAM with the new instruction and the dirty bit set
                    l1d_ctrl_d[17:0] <= {cpu_EXMEM2_addr_stored[25:10], 1'b1, 1'b1};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[9:3]; // DPRAM index, aligned on cache line size (8 words = 256 bits)
                    l1d_ctrl_we <= 1'b1;

                    // Update the cache line data at the correct offset
                    case (cpu_EXMEM2_addr_stored[2:0])
                        3'd0: begin
                            l1d_ctrl_d[49:18]    <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[273:50]   <= l1d_ctrl_q[273:50];
                        end
                        3'd1: begin
                            l1d_ctrl_d[81:50]    <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[49:18]    <= l1d_ctrl_q[49:18];
                            l1d_ctrl_d[273:82]   <= l1d_ctrl_q[273:82];
                        end
                        3'd2: begin
                            l1d_ctrl_d[113:82]   <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[81:18]    <= l1d_ctrl_q[81:18];
                            l1d_ctrl_d[273:114]  <= l1d_ctrl_q[273:114];
                        end
                        3'd3: begin
                            l1d_ctrl_d[145:114]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[113:18]   <= l1d_ctrl_q[113:18];
                            l1d_ctrl_d[273:146]  <= l1d_ctrl_q[273:146];
                        end
                        3'd4: begin
                            l1d_ctrl_d[177:146]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[145:18]   <= l1d_ctrl_q[145:18];
                            l1d_ctrl_d[273:178]  <= l1d_ctrl_q[273:178];
                        end
                        3'd5: begin
                            l1d_ctrl_d[209:178]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[177:18]   <= l1d_ctrl_q[177:18];
                            l1d_ctrl_d[273:210]  <= l1d_ctrl_q[273:210];
                        end
                        3'd6: begin
                            l1d_ctrl_d[241:210]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[209:18]   <= l1d_ctrl_q[209:18];
                            l1d_ctrl_d[273:242]  <= l1d_ctrl_q[273:242];
                        end
                        3'd7: begin
                            l1d_ctrl_d[273:242]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[241:18]   <= l1d_ctrl_q[241:18];
                        end
                    endcase

                    state <= STATE_L1D_WRITE_WRITE_TO_CACHE;
                end
                else
                begin
                    $display("%d: CacheController L1D WRITE CACHE MISS", $time);
                    // Cache miss
                    // If current line is dirty and needs to be evicted
                    if (l1d_ctrl_q[0])
                    begin
                        $display("%d: CacheController L1D write miss: current line is dirty, need to evict first", $time);
                        // Current cache line is dirty, need to write it back to memory first
                        if (app_rdy && app_wdf_rdy)
                        begin
                            app_cmd <= 3'b000; // WRITE command
                            app_en <= 1'b1;
                            // We need to write the address of the old cache line, so we need to use:
                            // the tag l1d_ctrl_q[17:2]
                            // the index of the cache line cpu_EXMEM2_addr_stored[9:3], which is aligned to 256 bits (8 words)
                            app_addr <= {l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[9:3]}; // Align to 256 bits (8 words)
                            app_wdf_wren <= 1'b1; // Enable write data
                            app_wdf_data <= l1d_ctrl_q[273:18]; // Data to write, which is the cache line data
                            app_wdf_end <= 1'b1; // End of write data

                            state <= STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD; // Wait for MIG7 to accept the write command
                        end
                        else
                        begin
                            // Go to state where we wait for MIG7 to be ready
                            state <= STATE_L1D_WRITE_MISS_EVICT_DIRTY_WAIT_READY;
                        end
                    end
                    // If current line not dirty and therefore can be safely overwritten
                    else
                    begin
                        $display("%d: CacheController L1D write miss: current line is clean, can fetch new line directly", $time);
                        // Current cache line is not dirty, can directly fetch new cache line
                        app_cmd <= 3'b001; // READ command
                        app_en <= 1'b1;
                        app_addr <= {4'd0, cpu_EXMEM2_addr_stored[31:3]}; // Align to 256 bits (8 words)
                        $display("%d: CacheController MIG7 L1D write miss read CMD: addr=0x%h", $time, {4'd0, cpu_EXMEM2_addr_stored[31:3]});

                        state <= STATE_L1D_WRITE_MISS_FETCH_WAIT_READY; // Wait for MIG7 to accept the read command
                    end
                end
            end

            STATE_L1D_WRITE_MISS_EVICT_DIRTY_WAIT_READY: begin
                // Wait for the MIG7 to be ready, so we can proceed with the write operation
                if (app_rdy && app_wdf_rdy)
                begin
                    app_cmd <= 3'b000; // WRITE command
                    app_en <= 1'b1;
                    // We need to write the address of the old cache line, so we need to use:
                    // the tag l1d_ctrl_q[17:2]
                    // the index of the cache line cpu_EXMEM2_addr_stored[9:3], which is aligned to 256 bits (8 words)
                    app_addr <= {l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[9:3]}; // Align to 256 bits (8 words)
                    app_wdf_wren <= 1'b1; // Enable write data
                    app_wdf_data <= cache_line_data; // Data to write, which is the cache line data
                    app_wdf_end <= 1'b1; // End of write data
                    $display("%d: CacheController MIG7 L1D write miss evict CMD: addr=0x%h, data=0x%h", $time, {l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[9:3]}, cache_line_data);
                    state <= STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD; // Wait for MIG7 to accept the write command
                end
            end

            STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD: begin
                // No matter if app_rdy, we can disassert app_wdf signals as it was ready the previous cycle
                app_wdf_wren <= 1'b0; // Disable write data
                app_wdf_end <= 1'b0; // End of write data
                // Wait for MIG7 to assert ready, indicating the write command is accepted
                if (app_rdy)
                begin
                    // MIG7 is ready, we can proceed with the read operation
                    app_en <= 1'b0; // Disassert app_en to prevent sending another command
                    state <= STATE_L1D_WRITE_MISS_FETCH_SEND_CMD; // The cache line is now written to MIG7, we can start a read the next cycle
                end
            end

            STATE_L1D_WRITE_MISS_FETCH_SEND_CMD: begin
                // Set the MIG7 read command for the new cache line
                app_cmd <= 3'b001; // READ command
                app_en <= 1'b1;
                app_addr <= {4'd0, cpu_EXMEM2_addr_stored[31:3]}; // Align to 256 bits (8 words)
                $display("%d: CacheController MIG7 L1D write miss fetch CMD: addr=0x%h", $time, {4'd0, cpu_EXMEM2_addr_stored[31:3]});

                state <= STATE_L1D_WRITE_MISS_FETCH_WAIT_READY; // Wait for MIG7 to accept the read command
            end

            STATE_L1D_WRITE_MISS_FETCH_WAIT_READY: begin
                // Wait for MIG7 to assert ready, indicating the read command is accepted
                if (app_rdy)
                begin
                    // MIG7 is ready, we can proceed with the read operation
                    app_en <= 1'b0; // Disassert app_en to prevent sending another command
                    state <= STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA; // Wait until the data is ready
                end
            end

            STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA: begin
                // Wait until data is ready, then update it with the write data and write it to DPRAM of L1D cache with dirty and valid bit set
                if (app_rd_data_valid && app_rd_data_end)
                begin
                    
                    l1d_ctrl_d[17:0] <= {cpu_EXMEM2_addr_stored[25:10], 1'b1, 1'b1};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[9:3]; // DPRAM index, aligned on cache line size (8 words = 256 bits)
                    l1d_ctrl_we <= 1'b1;

                    // Update the cache line data at the correct offset
                    case (cpu_EXMEM2_addr_stored[2:0])
                        3'd0: begin
                            l1d_ctrl_d[49:18]    <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[273:50]   <= app_rd_data[255:32];
                        end
                        3'd1: begin
                            l1d_ctrl_d[81:50]    <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[49:18]    <= app_rd_data[31:0];
                            l1d_ctrl_d[273:82]   <= app_rd_data[255:64];
                        end
                        3'd2: begin
                            l1d_ctrl_d[113:82]   <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[81:18]    <= app_rd_data[63:0];
                            l1d_ctrl_d[273:114]  <= app_rd_data[255:96];
                        end
                        3'd3: begin
                            l1d_ctrl_d[145:114]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[113:18]   <= app_rd_data[95:0];
                            l1d_ctrl_d[273:146]  <= app_rd_data[255:128];
                        end
                        3'd4: begin
                            l1d_ctrl_d[177:146]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[145:18]   <= app_rd_data[127:0];
                            l1d_ctrl_d[273:178]  <= app_rd_data[255:160];
                        end
                        3'd5: begin
                            l1d_ctrl_d[209:178]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[177:18]   <= app_rd_data[159:0];
                            l1d_ctrl_d[273:210]  <= app_rd_data[255:192];
                        end
                        3'd6: begin
                            l1d_ctrl_d[241:210]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[209:18]   <= app_rd_data[191:0];
                            l1d_ctrl_d[273:242]  <= app_rd_data[255:224];
                        end
                        3'd7: begin
                            l1d_ctrl_d[273:242]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[241:18]   <= app_rd_data[223:0];
                        end
                    endcase

                    state <= STATE_L1D_WRITE_WRITE_TO_CACHE;
                end
            end

            // TODO: Note that these two states are the same as for read atm. Might want to remove duplication if it stays this way
            STATE_L1D_WRITE_WRITE_TO_CACHE: begin
                // Start by disasserting the write enable
                l1d_ctrl_we <= 1'b0;

                // Set cpu_done
                cpu_EXMEM2_done <= 1'b1;
                $display("%d: CacheController EXMEM2 WRITE OPERATION COMPLETE: addr=0x%h, data=0x%h", $time, cpu_EXMEM2_addr_stored, cpu_EXMEM2_data_stored);
                state <= STATE_L1D_WRITE_SIGNAL_CPU_DONE; // Extra stage for the 50 MHz CPU to see the results
            end

            STATE_L1D_WRITE_SIGNAL_CPU_DONE: begin
                state <= STATE_IDLE; // After this stage, we can return to IDLE state
            end

        endcase
    end
end


endmodule
