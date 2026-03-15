/*
 * CacheController (SDRAM)
 * Cache controller for L1 instruction and data cache, to use with SDRAM via custom SDRAM controller
 * Runs at 100 MHz
 * Connects to:
 * - DPRAM for L1 instruction cache (hardcoded to 7 bits, or 128 lines)
 * - DPRAM for L1 data cache (hardcoded to 7 bits, or 128 lines)
 * - CPU pipeline for commands from FE2 and EXMEM2 stages
 * - SDRAM controller for memory access
 * 
 * Cache line format: {256bit_data, 14bit_tag, 1bit_valid} = 271 bits
 * - Tag is 14 bits to support 64MB address space (26-bit byte address - 7-bit index - 5-bit byte offset)
 *   Equivalently: addr[25:12] = tag, addr[11:5] = index, addr[4:2] = word offset, addr[1:0] = byte offset
 * - Dirty bit is stored separately in l1d_dirty_bits register array for fast access
 * 
 * NOTE: The SDRAM controller uses 256 bit (21-bit) addressing for 64MB
 *       SDRAM address = byte_addr[25:5] (strips the 5-bit cache line offset)
 */
module CacheController (
    // ---- System interface ----
    input  wire         clk100,
    input  wire         reset,

    // ---- CPU pipeline interface ----
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
    input  wire [3:0]   cpu_EXMEM2_byte_enable, // Byte enable for sub-word writes (1111=word, 0001=byte0, etc.)
    output reg          cpu_EXMEM2_done     = 1'b0,
    output reg  [31:0]  cpu_EXMEM2_result   = 32'd0, // Result of the data access

    // Cache clear interface
    input  wire         cpu_clear_cache,
    output reg          cpu_clear_cache_done = 1'b0,

    // ---- L1 cache DPRAM interface ----
    // Cache line format: {256bit_data, 14bit_tag, 1bit_valid} = 271 bits
    // Tag is 14 bits for 64MB address space (byte addr[25:12] = tag, [11:5] = index, [4:2] = word, [1:0] = byte)
    // L1i cache
    output reg  [270:0] l1i_ctrl_d          = 271'b0,
    output reg  [6:0]   l1i_ctrl_addr       = 7'b0,
    output reg          l1i_ctrl_we         = 1'b0,
    input  wire [270:0] l1i_ctrl_q,

    // L1d cache
    output reg  [270:0] l1d_ctrl_d          = 271'b0,
    output reg  [6:0]   l1d_ctrl_addr       = 7'b0,
    output reg          l1d_ctrl_we         = 1'b0,
    input  wire [270:0] l1d_ctrl_q,

    // ---- SDRAM controller interface ----
    output reg  [20:0]  sdc_addr            = 21'd0,
    output reg  [255:0] sdc_data            = 256'd0,
    output reg          sdc_we              = 1'b0,
    output reg          sdc_start           = 1'b0,
    input  wire         sdc_done,
    input  wire [255:0] sdc_q
);

// ---- Cache line format constants ----
// Cache line: {256bit_data, 14bit_tag, 1bit_valid} = 271 bits
localparam CACHE_VALID       = 0;       // Bit 0: valid flag
localparam CACHE_TAG_LO      = 1;       // Bits 14:1: tag (14 bits for 64MB)
localparam CACHE_TAG_HI      = 14;
localparam CACHE_DATA_LO     = 15;      // Bits 270:15: 256-bit data (8 x 32-bit words)
localparam CACHE_DATA_HI     = 270;
localparam CACHE_TAG_BITS    = 14;      // Number of tag bits
localparam CACHE_LINE_WORDS  = 8;       // 8 words per cache line

// ---- Helper functions ----

// Select a 32-bit word from a 256-bit cache line data portion
function [31:0] select_word;
    input [255:0] data;
    input [2:0] offset;
    begin
        case (offset)
            3'd0: select_word = data[31:0];
            3'd1: select_word = data[63:32];
            3'd2: select_word = data[95:64];
            3'd3: select_word = data[127:96];
            3'd4: select_word = data[159:128];
            3'd5: select_word = data[191:160];
            3'd6: select_word = data[223:192];
            3'd7: select_word = data[255:224];
        endcase
    end
endfunction

// Update a 32-bit word at offset within a 256-bit cache line data portion
function [255:0] update_word_in_line;
    input [255:0] line_data;
    input [31:0] new_word;
    input [2:0] offset;
    begin
        update_word_in_line = line_data;
        case (offset)
            3'd0: update_word_in_line[31:0]    = new_word;
            3'd1: update_word_in_line[63:32]   = new_word;
            3'd2: update_word_in_line[95:64]   = new_word;
            3'd3: update_word_in_line[127:96]  = new_word;
            3'd4: update_word_in_line[159:128] = new_word;
            3'd5: update_word_in_line[191:160] = new_word;
            3'd6: update_word_in_line[223:192] = new_word;
            3'd7: update_word_in_line[255:224] = new_word;
        endcase
    end
endfunction

// Merge bytes in a word based on byte_enable mask
// For each byte position: if byte_enable[i] is 1, use new_data byte; else keep old_data byte
function [31:0] merge_bytes;
    input [31:0] old_data;
    input [31:0] new_data;
    input [3:0]  byte_en;
    begin
        merge_bytes[7:0]   = byte_en[0] ? new_data[7:0]   : old_data[7:0];
        merge_bytes[15:8]  = byte_en[1] ? new_data[15:8]  : old_data[15:8];
        merge_bytes[23:16] = byte_en[2] ? new_data[23:16] : old_data[23:16];
        merge_bytes[31:24] = byte_en[3] ? new_data[31:24] : old_data[31:24];
    end
endfunction

// ---- State encoding ----
localparam
    STATE_IDLE                                  = 5'd0,

    // L1I cache miss
    STATE_L1I_WAIT_READ_DATA                    = 5'd1,
    STATE_L1I_WRITE_TO_CACHE                    = 5'd2,

    // L1I prefetch
    STATE_L1I_PREFETCH_CHECK                    = 5'd3,
    STATE_L1I_PREFETCH_WAIT_DATA                = 5'd4,
    STATE_L1I_PREFETCH_WRITE_TO_CACHE           = 5'd5,

    // L1D read (slow path: line might be dirty)
    STATE_L1D_READ_WAIT_CACHE_READ              = 5'd6,
    STATE_L1D_READ_CHECK_CACHE                  = 5'd7,
    STATE_L1D_READ_EVICT_DIRTY_SEND_CMD         = 5'd8,
    STATE_L1D_READ_SEND_CMD                     = 5'd9,
    STATE_L1D_READ_WAIT_DATA                    = 5'd10,
    STATE_L1D_READ_WRITE_TO_CACHE               = 5'd11,

    // L1D read (fast path: line is clean)
    STATE_L1D_READ_FAST_WAIT_DATA               = 5'd12,
    STATE_L1D_READ_FAST_WRITE_TO_CACHE          = 5'd13,

    // L1D write
    STATE_L1D_WRITE_WAIT_CACHE_READ             = 5'd14,
    STATE_L1D_WRITE_CHECK_CACHE                 = 5'd15,
    STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD   = 5'd16,
    STATE_L1D_WRITE_MISS_FETCH_SEND_CMD         = 5'd17,
    STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA        = 5'd18,
    STATE_L1D_WRITE_WRITE_TO_CACHE              = 5'd19,

    // Cache clear
    STATE_CLEARCACHE_REQUESTED                  = 5'd20,
    STATE_CLEARCACHE_L1I_CLEAR                  = 5'd21,
    STATE_CLEARCACHE_L1D_READ                   = 5'd22,
    STATE_CLEARCACHE_L1D_WAIT_READ              = 5'd23,
    STATE_CLEARCACHE_L1D_CHECK                  = 5'd24,
    STATE_CLEARCACHE_L1D_EVICT_SEND_CMD         = 5'd25,
    STATE_CLEARCACHE_L1D_CLEAR                  = 5'd26,
    STATE_CLEARCACHE_L1D_CLEAR_FINISH           = 5'd27,
    STATE_CLEARCACHE_DONE                       = 5'd28;

reg [4:0] state = STATE_IDLE;


// Storing requests from CPU pipeline when they arrive, to process them when state machine is ready for new requests
reg cpu_FE2_new_request = 1'b0;
reg [31:0] cpu_FE2_addr_stored = 32'd0;
reg [31:0] cpu_FE2_addr_in_flight = 32'd0; // Address captured when SDRAM request is sent
// For reference: cpu_FE2_cache_tag = cpu_FE2_addr_stored[25:12]
// For reference: cpu_FE2_cache_index = cpu_FE2_addr_stored[11:5]

reg cpu_EXMEM2_new_request = 1'b0;
reg [31:0] cpu_EXMEM2_addr_stored = 32'd0;
reg [31:0] cpu_EXMEM2_data_stored = 32'd0;
reg cpu_EXMEM2_we_stored = 1'b0;
reg [3:0]  cpu_EXMEM2_byte_enable_stored = 4'b1111;
// For reference: cpu_EXMEM2_cache_tag = cpu_EXMEM2_addr_stored[25:12]
// For reference: cpu_EXMEM2_cache_index = cpu_EXMEM2_addr_stored[11:5]

reg cpu_clear_cache_new_request = 1'b0;

// Cache clearing control registers
reg [6:0] clear_cache_index = 7'd0; // Index for iterating through cache lines (0-127)

// ---- L1I Prefetching Optimization ----
// Simple next-line prefetcher: after servicing an L1I miss, queue a prefetch
// of the next sequential cache line. The prefetch executes during true idle time.
// This doesn't reduce latency of the current miss, but prevents future sequential misses.
reg         l1i_prefetch_pending = 1'b0;     // Flag indicating a prefetch is waiting to be executed
reg [25:0]  l1i_prefetch_addr = 26'd0;       // 26-bit byte address of the cache line to prefetch (64MB)
// Note: we need 26 bits for the address (64MB = 2^26 bytes)
// The prefetch address is the next sequential cache line (addr + 32 bytes)
// ---- Dirty Bit Array Optimization ----
// Separate 128-bit register array to track dirty status of L1D cache lines
// This allows immediate (combinational) dirty check without waiting for DPRAM read
// Each bit corresponds to one cache line (index 0-127)
reg [127:0] l1d_dirty_bits = 128'b0;

// Wire for immediate dirty status check based on current address
// Uses the cache line index (bits [11:5] of byte address) to index into dirty array
wire l1d_line_is_dirty_fast = l1d_dirty_bits[cpu_EXMEM2_start ? cpu_EXMEM2_addr[11:5] : cpu_EXMEM2_addr_stored[11:5]];

reg ignore_fe2_result = 1'b0; // If a flush is received while processing a FE2 request, we need to ignore the result when it is done

reg get_address_after_ignore = 1'b0; // We need to delay storing the requested address if current request should be ignored

always @(posedge clk100)
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

        l1i_ctrl_d <= 271'b0;
        l1i_ctrl_addr <= 7'b0;
        l1i_ctrl_we <= 1'b0;
        l1d_ctrl_d <= 271'b0;
        l1d_ctrl_addr <= 7'b0;
        l1d_ctrl_we <= 1'b0;

        state <= STATE_IDLE;

        cpu_FE2_new_request <= 1'b0;
        cpu_FE2_addr_stored <= 32'd0;
        cpu_FE2_addr_in_flight <= 32'd0;
        cpu_EXMEM2_new_request <= 1'b0;
        cpu_EXMEM2_addr_stored <= 32'd0;
        cpu_EXMEM2_data_stored <= 32'd0;
        cpu_EXMEM2_we_stored <= 1'b0;
        cpu_EXMEM2_byte_enable_stored <= 4'b1111;

        cpu_clear_cache_new_request <= 1'b0;

        clear_cache_index <= 7'd0;
        cpu_clear_cache_done <= 1'b0;

        l1d_dirty_bits <= 128'b0; // Clear all dirty bits on reset

        l1i_prefetch_pending <= 1'b0; // Clear prefetch pending flag on reset
        l1i_prefetch_addr <= 24'd0;

        ignore_fe2_result <= 1'b0;
        get_address_after_ignore <= 1'b0;
    end
    else
    begin
        // Check for CPU FE2 request
        if (cpu_FE2_start)
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
        if (cpu_EXMEM2_start)                                                                                                                                  
        begin
            cpu_EXMEM2_new_request <= 1'b1;
            cpu_EXMEM2_addr_stored <= cpu_EXMEM2_addr;
            cpu_EXMEM2_data_stored <= cpu_EXMEM2_data;
            cpu_EXMEM2_we_stored <= cpu_EXMEM2_we;
            cpu_EXMEM2_byte_enable_stored <= cpu_EXMEM2_byte_enable;
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
            if (state == STATE_L1I_WAIT_READ_DATA)
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
                    $display("%0t Cache EXMEM2: addr=%05h we=%b data=%08h", $time, cpu_EXMEM2_start ? cpu_EXMEM2_addr : cpu_EXMEM2_addr_stored, cpu_EXMEM2_start ? cpu_EXMEM2_we : cpu_EXMEM2_we_stored, cpu_EXMEM2_start ? cpu_EXMEM2_data : cpu_EXMEM2_data_stored);

                    // Cancel any pending prefetch since we have a real data request
                    l1i_prefetch_pending <= 1'b0;

                    // Handle write request
                    if ((cpu_EXMEM2_new_request && cpu_EXMEM2_we_stored) || (cpu_EXMEM2_start && cpu_EXMEM2_we))
                    begin
                        // Read cache line first to determine if it needs to be evicted
                        l1d_ctrl_addr <= cpu_EXMEM2_start ? cpu_EXMEM2_addr[11:5] : cpu_EXMEM2_addr_stored[11:5];
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
                            sdc_addr <= cpu_EXMEM2_start ? cpu_EXMEM2_addr[25:5] : cpu_EXMEM2_addr_stored[25:5];
                            state <= STATE_L1D_READ_FAST_WAIT_DATA;
                        end
                        else
                        begin
                            // Line might be dirty - use slow path: read DPRAM first to get data for eviction
                            l1d_ctrl_addr <= cpu_EXMEM2_start ? cpu_EXMEM2_addr[11:5] : cpu_EXMEM2_addr_stored[11:5];
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
                    sdc_addr <= cpu_FE2_start ? cpu_FE2_addr[25:5] : cpu_FE2_addr_stored[25:5];
                    
                    // Capture the address being requested so we use the correct address for cache fill
                    // This is important because cpu_FE2_addr_stored can change while waiting for SDRAM
                    cpu_FE2_addr_in_flight <= cpu_FE2_start ? cpu_FE2_addr : cpu_FE2_addr_stored;
                    
                    //$display("%0t L1I SDC REQ: cpu_addr=%0d sdram_addr=%0d", 
                    //         $time, cpu_FE2_start ? cpu_FE2_addr : cpu_FE2_addr_stored,
                    //         cpu_FE2_start ? cpu_FE2_addr[25:5] : cpu_FE2_addr_stored[25:5]);

                    cpu_FE2_new_request <= 1'b0; // Clear the request flag

                    state <= STATE_L1I_WAIT_READ_DATA;

                end

                // L1I PREFETCH: Execute pending prefetch when truly idle
                // Only prefetch when there are no pending CPU requests
                else if (l1i_prefetch_pending)
                begin
                    // Read the target cache line to check if it's already in cache
                    l1i_ctrl_addr <= l1i_prefetch_addr[11:5]; // Cache index
                    l1i_ctrl_we <= 1'b0;
                    state <= STATE_L1I_PREFETCH_CHECK;
                end
            end

            // ---- L1 Instruction Cache Read States ----

            STATE_L1I_WAIT_READ_DATA: begin
                // Disassert request
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;
                // Wait for SDRAM controller to be done
                if (sdc_done)
                begin
                    // Extract the requested 32-bit word based on offset for the CPU return value
                    // Use addr_in_flight which is the address we actually requested from SDRAM
                    cpu_FE2_result <= select_word(sdc_q, cpu_FE2_addr_in_flight[4:2]);

                    // Write the retrieved cache line directly to DPRAM
                    // Format: {256bit_data, 14bit_tag, 1bit_valid}
                    // Use addr_in_flight to ensure we write to the correct cache line
                    l1i_ctrl_d <= {sdc_q, cpu_FE2_addr_in_flight[25:12], 1'b1};
                    l1i_ctrl_addr <= cpu_FE2_addr_in_flight[11:5];
                    l1i_ctrl_we <= 1'b1;

                    //$display("%0t L1I FILL: cpu_addr=%0d sdram_addr=%0d cache_line=%0d tag=%0d data=%h", 
                    //         $time, cpu_FE2_addr_in_flight, cpu_FE2_addr_in_flight[25:5], cpu_FE2_addr_in_flight[11:5], cpu_FE2_addr_in_flight[25:12], sdc_q);

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
                    // Next cache line is current address + 8 words (one cache line)
                    l1i_prefetch_pending <= 1'b1;
                    l1i_prefetch_addr <= cpu_FE2_addr_stored[25:0] + 26'd32; // Next cache line (32 bytes)
                    //$display("%d: CacheController FE2 OPERATION COMPLETE: addr=0x%h, result=0x%h", $time, cpu_FE2_addr_stored, cpu_FE2_result);
                end
                else
                begin
                    //$display("%d: CacheController FE2 OPERATION COMPLETE: addr=0x%h, result=0x%h -- BUT IGNORED DUE TO FLUSH", $time, cpu_FE2_addr_stored, cpu_FE2_result);
                    ignore_fe2_result <= 1'b0; // Clear the ignore flag
                end
                
                state <= STATE_IDLE;
            end


            // ---- L1I Prefetch States ----
            // Simple next-line prefetcher that runs during true idle time.

            STATE_L1I_PREFETCH_CHECK: begin
                // Wait one cycle for DPRAM read, then check if line is already in cache
                // If a CPU request comes in, abort prefetch and handle the request
                if (cpu_FE2_new_request || cpu_FE2_start || 
                    cpu_EXMEM2_new_request || cpu_EXMEM2_start ||
                    cpu_clear_cache_new_request)
                begin
                    // Abort prefetch - a real request has priority
                    l1i_prefetch_pending <= 1'b0;
                    state <= STATE_IDLE;
                end
                else
                begin
                    // Check if the prefetch target is already in cache
                    // Cache line format: {256bit_data[270:15], 14bit_tag[14:1], 1bit_valid[0]}
                    if (l1i_ctrl_q[CACHE_VALID] && (l1i_ctrl_q[CACHE_TAG_HI:CACHE_TAG_LO] == l1i_prefetch_addr[25:12]))
                    begin
                        // Already in cache - no need to prefetch
                        l1i_prefetch_pending <= 1'b0;
                        state <= STATE_IDLE;
                    end
                    else
                    begin
                        // Not in cache - fetch from SDRAM
                        sdc_we <= 1'b0;
                        sdc_start <= 1'b1;
                        sdc_addr <= l1i_prefetch_addr[25:5]; // 21-bit SDRAM address
                        state <= STATE_L1I_PREFETCH_WAIT_DATA;
                    end
                end
            end

            STATE_L1I_PREFETCH_WAIT_DATA: begin
                // Disassert sdc signals
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;
                
                // If a CPU request comes in while waiting, we can't abort mid-SDRAM-access
                // So we'll complete the prefetch but then immediately handle the request
                
                // Wait for SDRAM controller to be done
                if (sdc_done)
                begin
                    // Write the retrieved cache line directly to L1I DPRAM
                    // Format: {256bit_data, 14bit_tag, 1bit_valid}
                    l1i_ctrl_d <= {sdc_q, l1i_prefetch_addr[25:12], 1'b1};
                    l1i_ctrl_addr <= l1i_prefetch_addr[11:5]; // DPRAM index
                    l1i_ctrl_we <= 1'b1;
                    
                    //$display("%0t L1I PREFETCH FILL: prefetch_addr=%0d cache_line=%0d data=%h", 
                    //         $time, l1i_prefetch_addr, l1i_prefetch_addr[9:3], sdc_q);
                    
                    state <= STATE_L1I_PREFETCH_WRITE_TO_CACHE;
                end
            end

            STATE_L1I_PREFETCH_WRITE_TO_CACHE: begin
                // Disassert the write enable
                l1i_ctrl_we <= 1'b0;
                
                // Prefetch complete - mark as done and return to IDLE
                l1i_prefetch_pending <= 1'b0;
                
                state <= STATE_IDLE;
            end

            // ---- L1 Data Cache Read States ----

            STATE_L1D_READ_WAIT_CACHE_READ: begin
                // Wait one cycle for DPRAM read to complete
                state <= STATE_L1D_READ_CHECK_CACHE;
            end

            STATE_L1D_READ_CHECK_CACHE: begin
                // Cache line is in l1d_ctrl_q
                // Dirty status comes from l1d_dirty_bits array

                // We already know it is a cache miss, otherwise the CPU would not have requested the read
                // Check dirty bit from the array (we only reach this state if dirty bits said dirty)
                // Note: l1d_dirty_bits was already checked in STATE_IDLE, but we double-check here for safety
                if (l1d_dirty_bits[cpu_EXMEM2_addr_stored[11:5]])
                begin
                    //$display("%d: CacheController L1D cache line is dirty, need to evict first", $time);
                    sdc_we <= 1'b1;
                    sdc_start <= 1'b1;
                    // Write back the old cache line: address = {old_tag, index}
                    sdc_addr <= {l1d_ctrl_q[CACHE_TAG_HI:CACHE_TAG_LO], cpu_EXMEM2_addr_stored[11:5]};
                    sdc_data <= l1d_ctrl_q[CACHE_DATA_HI:CACHE_DATA_LO];

                    state <= STATE_L1D_READ_EVICT_DIRTY_SEND_CMD;

                end
                else
                begin
                    //$display("%d: CacheController L1D cache line is clean, can read new line directly", $time);
                    // If not dirty, we can directly read the new cache line
                    sdc_we <= 1'b0;
                    sdc_start <= 1'b1;
                    sdc_addr <= cpu_EXMEM2_addr_stored[25:5];

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
                sdc_addr <= cpu_EXMEM2_addr_stored[25:5];

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
                    cpu_EXMEM2_result <= select_word(sdc_q, cpu_EXMEM2_addr_stored[4:2]);

                    // Write the retrieved cache line directly to DPRAM
                    // Format: {256bit_data, 14bit_tag, 1bit_valid}
                    l1d_ctrl_d <= {sdc_q, cpu_EXMEM2_addr_stored[25:12], 1'b1};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[11:5];
                    l1d_ctrl_we <= 1'b1;

                    state <= STATE_L1D_READ_WRITE_TO_CACHE;
                end
            end

            STATE_L1D_READ_WRITE_TO_CACHE: begin
                // Start by disasserting the write enable
                l1d_ctrl_we <= 1'b0;

                // DIRTY BIT ARRAY UPDATE: Clear dirty bit (new line from SDRAM is clean)
                l1d_dirty_bits[cpu_EXMEM2_addr_stored[11:5]] <= 1'b0;

                // Set cpu_done
                cpu_EXMEM2_done <= 1'b1;
                //$display("%d: CacheController EXMEM2 READ OPERATION COMPLETE: addr=0x%h, result=0x%h", $time, cpu_EXMEM2_addr_stored, cpu_EXMEM2_result);
                state <= STATE_IDLE; // After this stage, we can return to IDLE state

            end

            // ---- L1D Read Fast Path States (Dirty Bit Array Optimization) ----
            // These states handle L1D read misses when we know the line is clean
            // Saves 2 cycles by skipping the DPRAM read for dirty check

            STATE_L1D_READ_FAST_WAIT_DATA: begin
                // Disassert sdc signals (was asserted in IDLE)
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;

                // Wait until data is ready from SDRAM
                if (sdc_done)
                begin
                    // Extract the requested 32-bit word based on offset for the CPU return value
                    cpu_EXMEM2_result <= select_word(sdc_q, cpu_EXMEM2_addr_stored[4:2]);

                    // Write the retrieved cache line directly to DPRAM
                    // Format: {256bit_data, 14bit_tag, 1bit_valid}
                    l1d_ctrl_d <= {sdc_q, cpu_EXMEM2_addr_stored[25:12], 1'b1};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[11:5];
                    l1d_ctrl_we <= 1'b1;

                    state <= STATE_L1D_READ_FAST_WRITE_TO_CACHE;
                end
            end

            STATE_L1D_READ_FAST_WRITE_TO_CACHE: begin
                // Disassert the write enable
                l1d_ctrl_we <= 1'b0;

                // DIRTY BIT ARRAY UPDATE: Clear dirty bit (new line from SDRAM is clean)
                l1d_dirty_bits[cpu_EXMEM2_addr_stored[11:5]] <= 1'b0;

                // Set cpu_done - data is ready for the CPU
                cpu_EXMEM2_done <= 1'b1;
                //$display("%d: CacheController L1D READ FAST PATH COMPLETE: addr=0x%h, result=0x%h", $time, cpu_EXMEM2_addr_stored, cpu_EXMEM2_result);
                
                state <= STATE_IDLE;
            end

            // ---- L1 Data Cache Write States ----

            STATE_L1D_WRITE_WAIT_CACHE_READ: begin
                // Wait one cycle for DPRAM read to complete
                state <= STATE_L1D_WRITE_CHECK_CACHE;
            end

            STATE_L1D_WRITE_CHECK_CACHE: begin
                // Cache line is in l1d_ctrl_q
                // Dirty status comes from l1d_dirty_bits array

                // Check for cache hit: valid bit is set and tag matches
                //$display("%d: CacheController L1D WRITE check cache: addr=0x%h, cache_line_valid=%b, cache_line_dirty=%b, cache_line_tag=0x%h, expected_tag=0x%h", $time, cpu_EXMEM2_addr_stored, l1d_ctrl_q[CACHE_VALID], l1d_dirty_bits[cpu_EXMEM2_addr_stored[11:5]], l1d_ctrl_q[CACHE_TAG_HI:CACHE_TAG_LO], cpu_EXMEM2_addr_stored[25:12]);
                if (l1d_ctrl_q[CACHE_VALID] && (l1d_ctrl_q[CACHE_TAG_HI:CACHE_TAG_LO] == cpu_EXMEM2_addr_stored[25:12]))
                begin
                    //$display("%d: CacheController L1D WRITE CACHE HIT", $time);
                    // Cache hit: merge bytes with existing word and write back to DPRAM (dirty bit is in array)
                    l1d_ctrl_d <= {update_word_in_line(l1d_ctrl_q[CACHE_DATA_HI:CACHE_DATA_LO], merge_bytes(select_word(l1d_ctrl_q[CACHE_DATA_HI:CACHE_DATA_LO], cpu_EXMEM2_addr_stored[4:2]), cpu_EXMEM2_data_stored, cpu_EXMEM2_byte_enable_stored), cpu_EXMEM2_addr_stored[4:2]), cpu_EXMEM2_addr_stored[25:12], 1'b1};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[11:5];
                    l1d_ctrl_we <= 1'b1;

                    state <= STATE_L1D_WRITE_WRITE_TO_CACHE;
                end
                else
                begin
                    //$display("%d: CacheController L1D WRITE CACHE MISS", $time);
                    // Cache miss
                    // If current line is dirty and needs to be evicted (check dirty bit array)
                    if (l1d_dirty_bits[cpu_EXMEM2_addr_stored[11:5]])
                    begin
                        //$display("%d: CacheController L1D write miss: current line is dirty, need to evict first", $time);
                        // Current cache line is dirty, need to write it back to memory first
                        sdc_we <= 1'b1;
                        sdc_start <= 1'b1;
                        // We need to write the address of the old cache line, so we need to use:
                        // Write back the old cache line: address = {old_tag, index}
                        sdc_addr <= {l1d_ctrl_q[CACHE_TAG_HI:CACHE_TAG_LO], cpu_EXMEM2_addr_stored[11:5]};
                        sdc_data <= l1d_ctrl_q[CACHE_DATA_HI:CACHE_DATA_LO];

                        state <= STATE_L1D_WRITE_MISS_EVICT_DIRTY_SEND_CMD;

                    end
                    // If current line not dirty and therefore can be safely overwritten
                    else
                    begin
                        //$display("%d: CacheController L1D write miss: current line is clean, can fetch new line directly", $time);
                        // Current cache line is not dirty, can directly fetch new cache line
                        sdc_we <= 1'b0;
                        sdc_start <= 1'b1;
                        sdc_addr <= cpu_EXMEM2_addr_stored[25:5];

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
                sdc_addr <= cpu_EXMEM2_addr_stored[25:5];

                state <= STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA;
            end

            STATE_L1D_WRITE_MISS_FETCH_WAIT_DATA: begin
                // Disassert sdc signals
                sdc_start <= 1'b0;
                sdc_addr <= 21'd0;

                // Wait until data is ready, then update it with the write data and write it to DPRAM of L1D cache
                // Dirty bit is stored in the array, not in the cache line
                if (sdc_done)
                begin
                    
                    l1d_ctrl_d <= {update_word_in_line(sdc_q, merge_bytes(select_word(sdc_q, cpu_EXMEM2_addr_stored[4:2]), cpu_EXMEM2_data_stored, cpu_EXMEM2_byte_enable_stored), cpu_EXMEM2_addr_stored[4:2]), cpu_EXMEM2_addr_stored[25:12], 1'b1};
                    l1d_ctrl_addr <= cpu_EXMEM2_addr_stored[11:5]; // DPRAM index, aligned on cache line size (8 words = 32 bytes)
                    l1d_ctrl_we <= 1'b1;

                    state <= STATE_L1D_WRITE_WRITE_TO_CACHE;
                end
            end

            STATE_L1D_WRITE_WRITE_TO_CACHE: begin
                // Start by disasserting the write enable
                l1d_ctrl_we <= 1'b0;

                // DIRTY BIT ARRAY UPDATE: Mark this cache line as dirty
                l1d_dirty_bits[cpu_EXMEM2_addr_stored[11:5]] <= 1'b1;

                // Set cpu_done
                cpu_EXMEM2_done <= 1'b1;
                //$display("%d: CacheController EXMEM2 WRITE OPERATION COMPLETE: addr=0x%h, data=0x%h", $time, cpu_EXMEM2_addr_stored, cpu_EXMEM2_data_stored);
                state <= STATE_IDLE;
            end

            // ---- Cache Clear States ----

            STATE_CLEARCACHE_REQUESTED: begin
                // Start clearing process by initializing the index and starting with L1i cache
                clear_cache_index <= 7'd0;
                //$display("%d: CacheController Starting cache clear process", $time);
                state <= STATE_CLEARCACHE_L1I_CLEAR;
            end

            STATE_CLEARCACHE_L1I_CLEAR: begin
                // Write zeros to L1i cache line at current index
                l1i_ctrl_d <= 271'b0; // All zeros: no data, no tag, not valid
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
                // Evict the dirty cache line to SDRAM
                sdc_we <= 1'b1;
                sdc_start <= 1'b1;
                // Construct address from {old_tag, index}
                sdc_addr <= {l1d_ctrl_q[CACHE_TAG_HI:CACHE_TAG_LO], clear_cache_index};
                sdc_data <= l1d_ctrl_q[CACHE_DATA_HI:CACHE_DATA_LO];
                
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
                l1d_ctrl_d <= 271'b0; // All zeros: no data, no tag, not valid
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
