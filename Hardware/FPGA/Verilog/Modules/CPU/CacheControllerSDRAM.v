/*
 * CacheController (SDRAM)
 * Cache controller for L1 instruction and data cache, to use with SDRAM via custom SDRAM controller
 * Runs at 100 MHz
 * - Aside from the CPU pipeline interface (50 MHz), everything else is also in the 100 MHz domain
 * Connects to:
 * - DPRAM for L1 instruction cache (hardcoded to 7 bits, or 128 lines)
 * - DPRAM for L1 data cache (hardcoded to 7 bits, or 128 lines)
 * - CPU pipeline for commands from FE2 and EXMEM2 stages
 * - SDRAM controller for memory access
 * NOTE: The SDRAM controller uses 256 bit addressing
 */
module CacheController (
    //========================
    // System interface
    //========================
    input  wire         clk100,
    input  wire         reset,

    //========================
    // CPU pipeline interface (50 MHz domain)
    //========================
    // FE2 stage
    input  wire         cpu_FE2_start,
    input  wire [31:0]  cpu_FE2_addr,       // Address in CPU words for instruction fetch
    input  wire         cpu_FE2_flush,      // CPU is flushed, do not set the done signal when the fetch completes
    output reg          cpu_FE2_done        = 1'b0,
    output reg  [31:0]  cpu_FE2_result      = 32'd0, // Result of the instruction fetch

    // EXMEM2 stage
    input  wire         cpu_EXMEM2_start,
    input  wire [31:0]  cpu_EXMEM2_addr,    // Address in CPU words for data access
    input  wire [31:0]  cpu_EXMEM2_data,
    input  wire         cpu_EXMEM2_we,
    output reg          cpu_EXMEM2_done     = 1'b0,
    output reg  [31:0]  cpu_EXMEM2_result   = 32'd0, // Result of the data access

    // Cache clear interface
    input  wire         cpu_clear_cache,
    output reg          cpu_clear_cache_done = 1'b0,

    //========================
    // L1 cache DPRAM interface
    //========================
    // L1i cache
    output reg  [273:0] l1i_ctrl_d          = 274'b0,
    output reg  [6:0]   l1i_ctrl_addr       = 7'b0,
    output reg          l1i_ctrl_we         = 1'b0,
    input  wire [273:0] l1i_ctrl_q,

    // L1d cache
    output reg  [273:0] l1d_ctrl_d          = 274'b0,
    output reg  [6:0]   l1d_ctrl_addr       = 7'b0,
    output reg          l1d_ctrl_we         = 1'b0,
    input  wire [273:0] l1d_ctrl_q,

    //========================
    // SDRAM controller interface
    //========================
    output reg  [20:0]  sdc_addr            = 21'd0,
    output reg  [255:0] sdc_data            = 256'd0,
    output reg          sdc_we              = 1'b0,
    output reg          sdc_start           = 1'b0,
    input  wire         sdc_done,
    input  wire [255:0] sdc_q
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
    STATE_UNUSED_1                       = 8'd7,
    STATE_L1D_READ_EVICT_DIRTY_SEND_CMD  = 8'd8,
    STATE_L1D_READ_SEND_CMD              = 8'd9,
    STATE_UNUSED_2                       = 8'd10,
    STATE_L1D_READ_WAIT_DATA             = 8'd11,
    STATE_L1D_READ_WRITE_TO_CACHE        = 8'd12,
    STATE_L1D_READ_SIGNAL_CPU_DONE       = 8'd13,

    // L1 Data Cache Write States
    STATE_L1D_WRITE_WAIT_CACHE_READ             = 8'd14,
    STATE_L1D_WRITE_CHECK_CACHE                 = 8'd15,
    STATE_UNUSED_3                              = 8'd16,
    STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD   = 8'd17,
    STATE_L1D_WRITE_MISS_FETCH_SEND_CMD         = 8'd18,
    STATE_UNUSED_4                              = 8'd19,
    STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA        = 8'd20,
    STATE_L1D_WRITE_WRITE_TO_CACHE              = 8'd21,
    STATE_L1D_WRITE_SIGNAL_CPU_DONE             = 8'd22,

    // Cache Clear States
    STATE_CLEARCACHE_REQUESTED                  = 8'd23,
    STATE_CLEARCACHE_L1I_CLEAR                  = 8'd24,
    STATE_CLEARCACHE_L1D_READ                   = 8'd25,
    STATE_CLEARCACHE_L1D_WAIT_READ              = 8'd26,
    STATE_CLEARCACHE_L1D_CHECK                  = 8'd27,
    STATE_UNUSED_5                              = 8'd28,
    STATE_CLEARCACHE_L1D_EVICT_SEND_CMD         = 8'd29,
    STATE_CLEARCACHE_L1D_CLEAR                  = 8'd30,
    STATE_CLEARCACHE_L1D_CLEAR_FINISH           = 8'd31,
    STATE_CLEARCACHE_DONE                       = 8'd32,
    
    // Optimized L1D Read States using dirty bit array
    // These states skip the DPRAM read when we already know the line is clean
    STATE_L1D_READ_FAST_WAIT_DATA               = 8'd33,  // Wait for SDRAM data (skipped dirty check)
    STATE_L1D_READ_FAST_WRITE_TO_CACHE          = 8'd34,  // Write to cache and signal done
    
    // L1I Prefetching States - DISABLED
    // Prefetching was causing timing issues with the CPU pipeline.
    // See STATE_L1I_SIGNAL_CPU_DONE for details.
    STATE_L1I_PREFETCH_START                    = 8'd35,  // UNUSED
    STATE_L1I_PREFETCH_WAIT_DATA                = 8'd36,  // UNUSED  
    STATE_L1I_PREFETCH_WRITE_TO_CACHE           = 8'd37;  // UNUSED


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

reg cpu_clear_cache_new_request = 1'b0;

// Cache clearing control registers
reg [6:0] clear_cache_index = 7'd0; // Index for iterating through cache lines (0-127)

//========================
// L1I Prefetching Optimization
//========================
// After servicing an L1I miss, automatically prefetch the next sequential cache line
// This improves performance for sequential code execution (which is the common case)
reg         l1i_prefetch_pending = 1'b0;     // Flag indicating a prefetch is waiting to be executed
reg [31:0]  l1i_prefetch_addr = 32'd0;       // Address of the cache line to prefetch
// Note: prefetch_addr uses the same format as cpu_FE2_addr (32-bit word address)
//========================
// Dirty Bit Array Optimization
//========================
// Separate 128-bit register array to track dirty status of L1D cache lines
// This allows immediate (combinational) dirty check without waiting for DPRAM read
// Each bit corresponds to one cache line (index 0-127)
reg [127:0] l1d_dirty_bits = 128'b0;

// Wire for immediate dirty status check based on current address
// Uses the cache line index (bits 9:3 of the address) to index into dirty array
wire l1d_line_is_dirty_fast = l1d_dirty_bits[cpu_EXMEM2_start ? cpu_EXMEM2_addr[9:3] : cpu_EXMEM2_addr_stored[9:3]];

reg ignore_fe2_result = 1'b0; // If a flush is received while processing a FE2 request, we need to ignore the result when it is done

reg get_address_after_ignore = 1'b0; // We need to delay storing the requested address if current request should be ignored

always @ (posedge clk100)
begin
    if (reset)
    begin
        cpu_FE2_done <= 1'b0;
        cpu_FE2_result <= 32'd0;
        cpu_EXMEM2_done <= 1'b0;
        cpu_EXMEM2_result <= 32'd0;
        
        sdc_addr <= 21'd0;
        sdc_data <= 256'd0;
        sdc_we <= 1'b0;
        sdc_start <= 1'b0;

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

        cpu_clear_cache_new_request <= 1'b0;

        clear_cache_index <= 7'd0;
        cpu_clear_cache_done <= 1'b0;

        l1d_dirty_bits <= 128'b0; // Clear all dirty bits on reset

        l1i_prefetch_pending <= 1'b0; // Clear prefetch pending flag on reset
        l1i_prefetch_addr <= 32'd0;

        ignore_fe2_result <= 1'b0;
        get_address_after_ignore <= 1'b0;
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
            if (ignore_fe2_result)
            begin
                get_address_after_ignore <= 1'b1; // We need to store the address after the ignore flag is cleared
            end
            else
            begin
                cpu_FE2_addr_stored <= cpu_FE2_addr;
            end
            //$display("%d: CacheController NEW FE2 REQUEST: addr=0x%h", $time, cpu_FE2_addr);
        end

        if (get_address_after_ignore && !ignore_fe2_result)
        begin
            cpu_FE2_addr_stored <= cpu_FE2_addr;
            get_address_after_ignore <= 1'b0;
        end

        // Check for CPU EXMEM2 request
        if (cpu_EXMEM2_start && !cpu_EXMEM2_start_prev)                                                                                                                                  
        begin
            cpu_EXMEM2_new_request <= 1'b1;
            cpu_EXMEM2_addr_stored <= cpu_EXMEM2_addr;
            cpu_EXMEM2_data_stored <= cpu_EXMEM2_data;
            cpu_EXMEM2_we_stored <= cpu_EXMEM2_we;
            //$display("%d: CacheController NEW EXMEM2 REQUEST: addr=0x%h, data=0x%h, we=%b", $time, cpu_EXMEM2_addr, cpu_EXMEM2_data, cpu_EXMEM2_we);
        end

        // Check for CPU cache clear request
        if (cpu_clear_cache)
        begin
            cpu_clear_cache_new_request <= 1'b1;
            //$display("%d: CacheController NEW CLEARCACHE REQUEST", $time);
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
                //$display("%d: CacheController FE2 FLUSH RECEIVED, will ignore result when done", $time);
            end
        end

        // State machine to handle requests
        case (state)
            STATE_IDLE: begin
                // Disassert signals on idle
                cpu_FE2_done <= 1'b0;
                cpu_EXMEM2_done <= 1'b0;
                cpu_clear_cache_done <= 1'b0;

                // Check if there is a cache clear request (highest priority)
                if (cpu_clear_cache_new_request)
                begin
                    // Cancel any pending prefetch
                    l1i_prefetch_pending <= 1'b0;
                    state <= STATE_CLEARCACHE_REQUESTED;
                end

                // Check if there is a new EXMEM2 request (priority over FE2)
                else if (cpu_EXMEM2_new_request || cpu_EXMEM2_start) // Also check for start to skip a cycle after idle
                begin
                    // Request can be either a read after cache miss, or a write which can be either a hit or a miss
                    cpu_EXMEM2_new_request <= 1'b0; // Clear the request flag
                    //$display("%0t Cache EXMEM2: addr=%05h we=%b data=%08h", $time, cpu_EXMEM2_start ? cpu_EXMEM2_addr : cpu_EXMEM2_addr_stored, cpu_EXMEM2_start ? cpu_EXMEM2_we : cpu_EXMEM2_we_stored, cpu_EXMEM2_start ? cpu_EXMEM2_data : cpu_EXMEM2_data_stored);

                    // Cancel any pending prefetch since we have a real data request
                    l1i_prefetch_pending <= 1'b0;

                    // Handle write request
                    if ((cpu_EXMEM2_new_request && cpu_EXMEM2_we_stored) || (cpu_EXMEM2_start && cpu_EXMEM2_we))
                    begin
                        // Read cache line first to determine if it needs to be evicted
                        l1d_ctrl_addr <= cpu_EXMEM2_start ? cpu_EXMEM2_addr[9:3] : cpu_EXMEM2_addr_stored[9:3];
                        l1d_ctrl_we <= 1'b0;
                        state <= STATE_L1D_WRITE_WAIT_CACHE_READ;
                    end

                    // Handle read request
                    else if ((cpu_EXMEM2_new_request && !cpu_EXMEM2_we_stored) || (cpu_EXMEM2_start && !cpu_EXMEM2_we))
                    begin
                        //$display("%d: CacheController EXMEM2 READ REQUEST", $time);
                        // DIRTY BIT ARRAY OPTIMIZATION:
                        // Use the fast dirty bit array to check if line is clean
                        // If clean, skip the DPRAM read and go directly to SDRAM fetch
                        if (!l1d_line_is_dirty_fast)
                        begin
                            // Line is clean - use fast path: skip DPRAM read, go directly to SDRAM
                            sdc_we <= 1'b0;
                            sdc_start <= 1'b1;
                            sdc_addr <= cpu_EXMEM2_start ? cpu_EXMEM2_addr[31:3] : cpu_EXMEM2_addr_stored[31:3];
                            state <= STATE_L1D_READ_FAST_WAIT_DATA;
                        end
                        else
                        begin
                            // Line might be dirty - use slow path: read DPRAM first to get data for eviction
                            l1d_ctrl_addr <= cpu_EXMEM2_start ? cpu_EXMEM2_addr[9:3] : cpu_EXMEM2_addr_stored[9:3];
                            l1d_ctrl_we <= 1'b0;
                            state <= STATE_L1D_READ_WAIT_CACHE_READ;
                        end
                    end
                end

                // Check if there is a new FE2 request (lower priority than EXMEM2)
                else if (cpu_FE2_new_request || cpu_FE2_start) // Also check for start to skip a cycle after idle
                begin
                    // Cancel any pending prefetch since we have a real request now
                    l1i_prefetch_pending <= 1'b0;

                    // FE2 requests are only READ operations
                    // Request sdram controller with arguments depending on the availability in the _stored registers
                    sdc_we <= 1'b0;
                    sdc_start <= 1'b1;
                    sdc_addr <= cpu_FE2_start ? cpu_FE2_addr[31:3] : cpu_FE2_addr_stored[31:3];

                    cpu_FE2_new_request <= 1'b0; // Clear the request flag

                    state <= STATE_L1I_WAIT_READ_DATA;

                end

                // L1I PREFETCH OPTIMIZATION: Disabled - see STATE_L1I_SIGNAL_CPU_DONE comment
                // L1I prefetching is disabled (see STATE_L1I_SIGNAL_CPU_DONE)
            end

            // ------------------------
            // L1 Instruction Cache Read States
            // ------------------------

            STATE_L1I_WAIT_READ_DATA: begin
                // Disassert request
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;
                // Wait for SDRAM controller to be done
                if (sdc_done)
                begin
                    // Extract the requested 32-bit word based on offset for the CPU return value
                    case (cpu_FE2_addr_stored[2:0])
                        3'd0: cpu_FE2_result <= sdc_q[31:0];
                        3'd1: cpu_FE2_result <= sdc_q[63:32];
                        3'd2: cpu_FE2_result <= sdc_q[95:64];
                        3'd3: cpu_FE2_result <= sdc_q[127:96];
                        3'd4: cpu_FE2_result <= sdc_q[159:128];
                        3'd5: cpu_FE2_result <= sdc_q[191:160];
                        3'd6: cpu_FE2_result <= sdc_q[223:192];
                        3'd7: cpu_FE2_result <= sdc_q[255:224];
                    endcase

                    // Write the retrieved cache line directly to DPRAM
                    // Format: {256bit_data, 16bit_tag, 1'b1(valid), 1'b0(dirty)}
                    l1i_ctrl_d <= {sdc_q, cpu_FE2_addr_stored[25:10], 1'b1, 1'b0};
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
                    //$display("%d: CacheController FE2 OPERATION COMPLETE: addr=0x%h, result=0x%h", $time, cpu_FE2_addr_stored, cpu_FE2_result);
                end
                else
                begin
                    //$display("%d: CacheController FE2 OPERATION COMPLETE: addr=0x%h, result=0x%h -- BUT IGNORED DUE TO FLUSH", $time, cpu_FE2_addr_stored, cpu_FE2_result);
                    ignore_fe2_result <= 1'b0; // Clear the ignore flag
                end
                state <= STATE_L1I_SIGNAL_CPU_DONE; // Extra stage for the 50 MHz CPU to see the results
            end

            STATE_L1I_SIGNAL_CPU_DONE: begin
                // L1I PREFETCH OPTIMIZATION:
                // DISABLED - causes 7 B32CC test failures
                // Root cause: The prefetch reduces L1I miss latency, which causes the CPU to execute
                // faster. This faster execution appears to expose a timing-related bug, possibly in
                // the CPU's hazard detection or forwarding logic. When instructions execute faster,
                // data dependencies are not being properly handled.
                // 
                // TODO: Investigate CPU pipeline hazard detection timing with reduced L1I latency.
                // The issue manifests as register values not being forwarded correctly when
                // subsequent instructions execute too quickly after a preceding ALU operation.
                
                state <= STATE_IDLE;
            end

            // ------------------------
            // L1I Prefetch States - DISABLED
            // These states are defined but currently unused due to timing issues.
            // See STATE_L1I_SIGNAL_CPU_DONE for details on why prefetching is disabled.
            // ------------------------

            STATE_L1I_PREFETCH_WAIT_DATA: begin
                // Disassert sdc signals
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;
                
                // Wait for SDRAM controller to be done
                if (sdc_done)
                begin
                    // Write the retrieved cache line directly to L1I DPRAM
                    // Format: {256bit_data, 16bit_tag, 1'b1(valid), 1'b0(dirty)}
                    l1i_ctrl_d <= {sdc_q, l1i_prefetch_addr[25:10], 1'b1, 1'b0};
                    l1i_ctrl_addr <= l1i_prefetch_addr[9:3]; // DPRAM index
                    l1i_ctrl_we <= 1'b1;
                    
                    state <= STATE_L1I_PREFETCH_WRITE_TO_CACHE;
                end
            end

            STATE_L1I_PREFETCH_WRITE_TO_CACHE: begin
                // Disassert the write enable
                l1i_ctrl_we <= 1'b0;
                
                // Prefetch complete - return to IDLE
                // Note: we do NOT signal cpu_done, this was a background operation
                
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

                // We already know it is a cache miss, otherwise the CPU would not have requested the read, so we only need to check for the dirty bit
                if (l1d_ctrl_q[0])
                begin
                    //$display("%d: CacheController L1D cache line is dirty, need to evict first", $time);
                    sdc_we <= 1'b1;
                    sdc_start <= 1'b1;
                    // We need to write the address of the old cache line, so we need to use:
                    // the tag l1d_ctrl_q[17:2]
                    // the index of the cache line cpu_EXMEM2_addr_stored[9:3], which is aligned to 256 bits (8 words)
                    sdc_addr <= {l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[9:3]};
                    sdc_data <= l1d_ctrl_q[273:18]; // Data to write, which is the cache line data

                    state <= STATE_L1D_READ_EVICT_DIRTY_SEND_CMD;

                end
                else
                begin
                    //$display("%d: CacheController L1D cache line is clean, can read new line directly", $time);
                    // If not dirty, we can directly read the new cache line
                    sdc_we <= 1'b0;
                    sdc_start <= 1'b1;
                    sdc_addr <= cpu_EXMEM2_addr_stored[31:3];

                    state <= STATE_L1D_READ_WAIT_DATA;
                end
            end

            STATE_L1D_READ_EVICT_DIRTY_SEND_CMD: begin
                // Disassert sdc signals
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;
                sdc_data <= 256'd0;
                sdc_we <= 1'b0;

                // Wait SDRAM controller to finish before proceeding
                if (sdc_done)
                begin
                    state <= STATE_L1D_READ_SEND_CMD; // The cache line is now written to SDRAM, we can start a read the next cycle
                end
            end

            STATE_L1D_READ_SEND_CMD: begin
                sdc_we <= 1'b0;
                sdc_start <= 1'b1;
                sdc_addr <= cpu_EXMEM2_addr_stored[31:3];

                state <= STATE_L1D_READ_WAIT_DATA;
            end

            STATE_L1D_READ_WAIT_DATA: begin
                // Disassert sdc signals
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;

                // Wait until data is ready, and write it to DPRAM of L1D cache
                if (sdc_done)
                begin
                    // Extract the requested 32-bit word based on offset for the CPU return value
                    case (cpu_EXMEM2_addr_stored[2:0])
                        3'd0: cpu_EXMEM2_result <= sdc_q[31:0];
                        3'd1: cpu_EXMEM2_result <= sdc_q[63:32];
                        3'd2: cpu_EXMEM2_result <= sdc_q[95:64];
                        3'd3: cpu_EXMEM2_result <= sdc_q[127:96];
                        3'd4: cpu_EXMEM2_result <= sdc_q[159:128];
                        3'd5: cpu_EXMEM2_result <= sdc_q[191:160];
                        3'd6: cpu_EXMEM2_result <= sdc_q[223:192];
                        3'd7: cpu_EXMEM2_result <= sdc_q[255:224];
                    endcase

                    // Write the retrieved cache line directly to DPRAM
                    // Format: {256bit_data, 16bit_tag, 1'b1(valid), 1'b0(dirty)}
                    l1d_ctrl_d <= {sdc_q, cpu_EXMEM2_addr_stored[25:10], 1'b1, 1'b0};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[9:3]; // DPRAM index, aligned on cache line size (8 words = 256 bits)
                    l1d_ctrl_we <= 1'b1;

                    state <= STATE_L1D_READ_WRITE_TO_CACHE; // Wait until the data is written so that the fetch of the next instruction can use the cache
                end
            end

            STATE_L1D_READ_WRITE_TO_CACHE: begin
                // Start by disasserting the write enable
                l1d_ctrl_we <= 1'b0;

                // DIRTY BIT ARRAY UPDATE: Clear dirty bit (new line from SDRAM is clean)
                l1d_dirty_bits[cpu_EXMEM2_addr_stored[9:3]] <= 1'b0;

                // Set cpu_done
                cpu_EXMEM2_done <= 1'b1;
                //$display("%d: CacheController EXMEM2 READ OPERATION COMPLETE: addr=0x%h, result=0x%h", $time, cpu_EXMEM2_addr_stored, cpu_EXMEM2_result);
                state <= STATE_L1D_READ_SIGNAL_CPU_DONE; // Extra stage for the 50 MHz CPU to see the results

            end

            STATE_L1D_READ_SIGNAL_CPU_DONE: begin
                state <= STATE_IDLE; // After this stage, we can return to IDLE state
            end

            // ------------------------
            // L1D Read Fast Path States (Dirty Bit Array Optimization)
            // These states handle L1D read misses when we know the line is clean
            // Saves 2 cycles by skipping the DPRAM read for dirty check
            // ------------------------

            STATE_L1D_READ_FAST_WAIT_DATA: begin
                // Disassert sdc signals (was asserted in IDLE)
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;

                // Wait until data is ready from SDRAM
                if (sdc_done)
                begin
                    // Extract the requested 32-bit word based on offset for the CPU return value
                    case (cpu_EXMEM2_addr_stored[2:0])
                        3'd0: cpu_EXMEM2_result <= sdc_q[31:0];
                        3'd1: cpu_EXMEM2_result <= sdc_q[63:32];
                        3'd2: cpu_EXMEM2_result <= sdc_q[95:64];
                        3'd3: cpu_EXMEM2_result <= sdc_q[127:96];
                        3'd4: cpu_EXMEM2_result <= sdc_q[159:128];
                        3'd5: cpu_EXMEM2_result <= sdc_q[191:160];
                        3'd6: cpu_EXMEM2_result <= sdc_q[223:192];
                        3'd7: cpu_EXMEM2_result <= sdc_q[255:224];
                    endcase

                    // Write the retrieved cache line directly to DPRAM
                    // Format: {256bit_data, 16bit_tag, 1'b1(valid), 1'b0(dirty)}
                    // Note: dirty bit is 0 because we just fetched fresh data from SDRAM
                    l1d_ctrl_d <= {sdc_q, cpu_EXMEM2_addr_stored[25:10], 1'b1, 1'b0};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[9:3];
                    l1d_ctrl_we <= 1'b1;

                    state <= STATE_L1D_READ_FAST_WRITE_TO_CACHE;
                end
            end

            STATE_L1D_READ_FAST_WRITE_TO_CACHE: begin
                // Disassert the write enable
                l1d_ctrl_we <= 1'b0;

                // DIRTY BIT ARRAY UPDATE: Clear dirty bit (new line from SDRAM is clean)
                l1d_dirty_bits[cpu_EXMEM2_addr_stored[9:3]] <= 1'b0;

                // Set cpu_done - data is ready for the CPU
                cpu_EXMEM2_done <= 1'b1;
                //$display("%d: CacheController L1D READ FAST PATH COMPLETE: addr=0x%h, result=0x%h", $time, cpu_EXMEM2_addr_stored, cpu_EXMEM2_result);
                
                // Go directly to IDLE (skip SIGNAL_CPU_DONE state for even faster return)
                // Wait, we need to keep the done signal high for a cycle for the 50MHz CPU
                state <= STATE_L1D_READ_SIGNAL_CPU_DONE;
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

                // Check for cache hit: valid bit is set and tag matches
                //$display("%d: CacheController L1D WRITE check cache: addr=0x%h, cache_line_valid=%b, cache_line_dirty=%b, cache_line_tag=0x%h, expected_tag=0x%h", $time, cpu_EXMEM2_addr_stored, l1d_ctrl_q[1], l1d_ctrl_q[0], l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[25:10]);
                if (l1d_ctrl_q[1] && (l1d_ctrl_q[17:2] == cpu_EXMEM2_addr_stored[25:10]))
                begin
                    //$display("%d: CacheController L1D WRITE CACHE HIT", $time);
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
                    //$display("%d: CacheController L1D WRITE CACHE MISS", $time);
                    // Cache miss
                    // If current line is dirty and needs to be evicted
                    if (l1d_ctrl_q[0])
                    begin
                        //$display("%d: CacheController L1D write miss: current line is dirty, need to evict first", $time);
                        // Current cache line is dirty, need to write it back to memory first
                        sdc_we <= 1'b1;
                        sdc_start <= 1'b1;
                        // We need to write the address of the old cache line, so we need to use:
                        // the tag l1d_ctrl_q[17:2]
                        // the index of the cache line cpu_EXMEM2_addr_stored[9:3], which is aligned to 256 bits (8 words)
                        sdc_addr <= {l1d_ctrl_q[17:2], cpu_EXMEM2_addr_stored[9:3]};
                        sdc_data <= l1d_ctrl_q[273:18]; // Data to write, which is the cache line data

                        state <= STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD;

                    end
                    // If current line not dirty and therefore can be safely overwritten
                    else
                    begin
                        //$display("%d: CacheController L1D write miss: current line is clean, can fetch new line directly", $time);
                        // Current cache line is not dirty, can directly fetch new cache line
                        sdc_we <= 1'b0;
                        sdc_start <= 1'b1;
                        sdc_addr <= cpu_EXMEM2_addr_stored[31:3];

                        state <= STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA;
                    end
                end
            end

            STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD: begin
                // Disassert sdc signals
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;
                sdc_data <= 256'd0;
                sdc_we <= 1'b0;

                // Wait SDRAM controller to finish before proceeding
                if (sdc_done)
                begin
                    state <= STATE_L1D_WRITE_MISS_FETCH_SEND_CMD; // The cache line is now written to SDRAM, we can start a read the next cycle
                end
            end

            STATE_L1D_WRITE_MISS_FETCH_SEND_CMD: begin
                sdc_we <= 1'b0;
                sdc_start <= 1'b1;
                sdc_addr <= cpu_EXMEM2_addr_stored[31:3];

                state <= STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA;
            end

            STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA: begin
                // Disassert sdc signals
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;

                // Wait until data is ready, then update it with the write data and write it to DPRAM of L1D cache with dirty and valid bit set
                if (sdc_done)
                begin
                    
                    l1d_ctrl_d[17:0] <= {cpu_EXMEM2_addr_stored[25:10], 1'b1, 1'b1};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[9:3]; // DPRAM index, aligned on cache line size (8 words = 256 bits)
                    l1d_ctrl_we <= 1'b1;

                    // Update the cache line data at the correct offset
                    case (cpu_EXMEM2_addr_stored[2:0])
                        3'd0: begin
                            l1d_ctrl_d[49:18]    <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[273:50]   <= sdc_q[255:32];
                        end
                        3'd1: begin
                            l1d_ctrl_d[81:50]    <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[49:18]    <= sdc_q[31:0];
                            l1d_ctrl_d[273:82]   <= sdc_q[255:64];
                        end
                        3'd2: begin
                            l1d_ctrl_d[113:82]   <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[81:18]    <= sdc_q[63:0];
                            l1d_ctrl_d[273:114]  <= sdc_q[255:96];
                        end
                        3'd3: begin
                            l1d_ctrl_d[145:114]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[113:18]   <= sdc_q[95:0];
                            l1d_ctrl_d[273:146]  <= sdc_q[255:128];
                        end
                        3'd4: begin
                            l1d_ctrl_d[177:146]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[145:18]   <= sdc_q[127:0];
                            l1d_ctrl_d[273:178]  <= sdc_q[255:160];
                        end
                        3'd5: begin
                            l1d_ctrl_d[209:178]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[177:18]   <= sdc_q[159:0];
                            l1d_ctrl_d[273:210]  <= sdc_q[255:192];
                        end
                        3'd6: begin
                            l1d_ctrl_d[241:210]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[209:18]   <= sdc_q[191:0];
                            l1d_ctrl_d[273:242]  <= sdc_q[255:224];
                        end
                        3'd7: begin
                            l1d_ctrl_d[273:242]  <= cpu_EXMEM2_data_stored;
                            l1d_ctrl_d[241:18]   <= sdc_q[223:0];
                        end
                    endcase

                    state <= STATE_L1D_WRITE_WRITE_TO_CACHE;
                end
            end

            STATE_L1D_WRITE_WRITE_TO_CACHE: begin
                // Start by disasserting the write enable
                l1d_ctrl_we <= 1'b0;

                // DIRTY BIT ARRAY UPDATE: Mark this cache line as dirty
                l1d_dirty_bits[cpu_EXMEM2_addr_stored[9:3]] <= 1'b1;

                // Set cpu_done
                cpu_EXMEM2_done <= 1'b1;
                //$display("%d: CacheController EXMEM2 WRITE OPERATION COMPLETE: addr=0x%h, data=0x%h", $time, cpu_EXMEM2_addr_stored, cpu_EXMEM2_data_stored);
                state <= STATE_L1D_WRITE_SIGNAL_CPU_DONE; // Extra stage for the 50 MHz CPU to see the results
            end

            STATE_L1D_WRITE_SIGNAL_CPU_DONE: begin
                state <= STATE_IDLE; // After this stage, we can return to IDLE state
            end

            // ------------------------
            // Cache Clear States
            // ------------------------

            STATE_CLEARCACHE_REQUESTED: begin
                // Start clearing process by initializing the index and starting with L1i cache
                clear_cache_index <= 7'd0;
                //$display("%d: CacheController Starting cache clear process", $time);
                state <= STATE_CLEARCACHE_L1I_CLEAR;
            end

            STATE_CLEARCACHE_L1I_CLEAR: begin
                // Write zeros to L1i cache line at current index
                l1i_ctrl_d <= 274'b0; // All zeros: no data, no tag, not valid, not dirty
                l1i_ctrl_addr <= clear_cache_index;
                l1i_ctrl_we <= 1'b1;
                
                // Check if we've cleared all L1i cache lines (0-127)
                if (clear_cache_index == 7'd127)
                begin
                    // Finished L1i cache, start with L1d cache
                    clear_cache_index <= 7'd0;
                    // We do not disable write here to not skip the last write
                    state <= STATE_CLEARCACHE_L1D_READ;
                    //$display("%d: CacheController L1i cache cleared, starting L1d cache", $time);
                end
                else
                begin
                    // Move to next L1i cache line
                    clear_cache_index <= clear_cache_index + 1'b1;
                end
            end

            STATE_CLEARCACHE_L1D_READ: begin
                // Disable L1i write if it was still enabled
                l1i_ctrl_we <= 1'b0;
                
                // DIRTY BIT ARRAY OPTIMIZATION:
                // Use the fast dirty bit array to skip reading clean lines
                if (l1d_dirty_bits[clear_cache_index])
                begin
                    // Line is dirty - read from DPRAM to get data for eviction
                    l1d_ctrl_addr <= clear_cache_index;
                    l1d_ctrl_we <= 1'b0;
                    state <= STATE_CLEARCACHE_L1D_WAIT_READ;
                end
                else
                begin
                    // Line is clean - skip the read and move to next line or clear phase
                    if (clear_cache_index == 7'd127)
                    begin
                        clear_cache_index <= 7'd0;
                        state <= STATE_CLEARCACHE_L1D_CLEAR;
                    end
                    else
                    begin
                        clear_cache_index <= clear_cache_index + 1'b1;
                        // Stay in this state to check next line
                    end
                end
            end

            STATE_CLEARCACHE_L1D_WAIT_READ: begin
                // Wait one cycle for DPRAM read to complete
                state <= STATE_CLEARCACHE_L1D_CHECK;
            end

            STATE_CLEARCACHE_L1D_CHECK: begin
                // We only get here if l1d_dirty_bits indicated the line is dirty
                // Evict the cache line to SDRAM
                // tag = l1d_ctrl_q[17:2]
                // data = l1d_ctrl_q[273:18]
                
                sdc_we <= 1'b1;
                sdc_start <= 1'b1;
                // Construct address from tag and index
                sdc_addr <= {l1d_ctrl_q[17:2], clear_cache_index};
                sdc_data <= l1d_ctrl_q[273:18];
                
                state <= STATE_CLEARCACHE_L1D_EVICT_SEND_CMD;
            end

            STATE_CLEARCACHE_L1D_EVICT_SEND_CMD: begin
                // Disassert sdc signals
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;
                sdc_data <= 256'd0;
                sdc_we <= 1'b0;

                // Wait SDRAM controller to finish before proceeding
                if (sdc_done)
                begin
                    // Move to next cache line
                    if (clear_cache_index == 7'd127)
                    begin
                        // Finished evicting all dirty L1d lines, start clearing
                        clear_cache_index <= 7'd0;
                        state <= STATE_CLEARCACHE_L1D_CLEAR;
                        //$display("%d: CacheController All dirty L1d lines evicted, starting L1d clear", $time);
                    end
                    else
                    begin
                        clear_cache_index <= clear_cache_index + 1'b1;
                        state <= STATE_CLEARCACHE_L1D_READ;
                    end
                end

               
            end

            STATE_CLEARCACHE_L1D_CLEAR: begin
                // Write zeros to L1d cache line at current index
                l1d_ctrl_d <= 274'b0; // All zeros: no data, no tag, not valid, not dirty
                l1d_ctrl_addr <= clear_cache_index;
                l1d_ctrl_we <= 1'b1;
                
                // Check if we've cleared all L1d cache lines (0-127)
                if (clear_cache_index == 7'd127)
                begin
                    // Finished clearing L1d cache, go to finish state to disable write enable
                    state <= STATE_CLEARCACHE_L1D_CLEAR_FINISH;
                    //$display("%d: CacheController L1d cache cleared, going to finish state", $time);
                end
                else
                begin
                    // Move to next L1d cache line
                    clear_cache_index <= clear_cache_index + 1'b1;
                end
            end

            STATE_CLEARCACHE_L1D_CLEAR_FINISH: begin
                // Disable write enable to ensure the last write completes properly
                l1d_ctrl_we <= 1'b0;
                
                // DIRTY BIT ARRAY UPDATE: Clear all dirty bits
                l1d_dirty_bits <= 128'b0;
                
                // Clear the cache clear request flag and return to idle via extra state
                cpu_clear_cache_new_request <= 1'b0;
                clear_cache_index <= 7'd0;
                cpu_clear_cache_done <= 1'b1;
                state <= STATE_CLEARCACHE_DONE;
                //$display("%d: CacheController Cache clear process completed", $time);
            end

            STATE_CLEARCACHE_DONE: begin
                state <= STATE_IDLE; // Return to idle state
            end

        endcase
    end
end


endmodule
