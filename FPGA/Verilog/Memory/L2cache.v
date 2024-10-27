/*
* L2 Cache
* Sits between CPU and SDRAM controller
* Made to run at 100MHz
*/
module L2cache(
    // clock/reset inputs
    input               clk,
    input               reset,

    // CPU bus
    input [23:0]        l2_addr,
    input [31:0]        l2_data,
    input               l2_we,
    input               l2_start,
    output [31:0]       l2_q,
    output              l2_done,

    // SDRAM controller bus
    output [23:0]       sdc_addr,
    output [31:0]       sdc_data,
    output              sdc_we,
    output              sdc_start,
    input [31:0]        sdc_q,
    input               sdc_done
);

wire cache_reset;
assign cache_reset = 1'b0;

parameter cache_size = 1024;                // cache size in words. 8129*4bytes = 32KiB
parameter index_size = 10;                  // index size: log2(cache_size)
parameter tag_size = 14;                    // mem_add_bits-index_size = 24-13 = 11
parameter cache_line_size = tag_size+32+1;  // tag + word + valid bit

reg [cache_line_size-1:0] cache [0:cache_size-1];   // cache memory

integer i;
// init cache to all zeros
initial
begin
    for (i = 0; i < cache_size; i = i + 1)
    begin
        cache[i] = 47'd0;
    end
end

reg [index_size-1:0]        cache_addr = 10'd0;
reg [cache_line_size-1:0]   cache_d = 47'd0;
reg                         cache_we = 1'b0;
reg [cache_line_size-1:0]   cache_q = 47'd0;
always @(posedge clk) 
begin
    cache_q <= cache[cache_addr];
    if (cache_we)
    begin
        cache[cache_addr] <= cache_d;
        $display("%d: wrote to l2 cache", $time);
    end
end

reg [31:0]              l2_q_reg = 32'd0;
reg                     l2_done_reg = 1'b0;
reg [23:0]              sdc_addr_reg = 24'd0;
reg [31:0]              sdc_data_reg = 32'd0;
reg                     sdc_we_reg = 1'b0;
reg                     sdc_start_reg = 1'b0;

reg start_prev = 1'b0;

// state machine
reg [2:0] state = 3'd0; // 0-7 states limit
parameter state_init            = 3'd0;
parameter state_idle            = 3'd1;
parameter state_writing         = 3'd2;
parameter state_check_cache     = 3'd3;
parameter state_miss_read_ram   = 3'd4;
parameter state_delay_cache     = 3'd5;
parameter state_done_high       = 3'd6;
parameter state_clear_cache     = 3'd7;

reg [31:0] addr_prev = 32'd0;

reg [15:0] clear_cache_counter = 16'd0; // 64k max

reg start_registered = 1'b0;

always @(posedge clk) 
begin
    if (reset)
    begin
        l2_q_reg <= 32'd0;
        l2_done_reg <= 1'b0;
        sdc_addr_reg <= 24'd0;
        sdc_data_reg <= 32'd0;
        sdc_we_reg <= 1'b0;
        sdc_start_reg <= 1'b0;

        addr_prev <= 32'd0;
        
        // Make sure the next cycle a new request can be detected!
        start_prev <= 1'b0;
        state <= state_clear_cache;

        clear_cache_counter <= 16'd0;

        start_registered <= 1'b0;
    end
    else
    begin
        addr_prev <= l2_addr;
        start_prev <= l2_start;
        l2_done_reg <= 1'b0;
        cache_we <= 1'b0;
        

        // NOTE: make sure to use latched l2_addr from rising start to make sure all addresses are correct!
        //  as l2_addr can change during a clear/skipresult (e.g. when jump or other pipeline flush)

        case(state)
            state_init: 
            begin
                state <= state_clear_cache;
            end

            state_clear_cache:
            begin
                if (l2_addr < 27'h800000 && ( (l2_start && !start_prev) || addr_prev >= 27'h800000 && l2_start) )
                begin
                    start_registered <= 1'b1;
                end

                if (clear_cache_counter == cache_size)
                begin
                    clear_cache_counter <= 16'd0;
                    state <= state_idle;
                end
                else
                begin
                    clear_cache_counter <= clear_cache_counter + 1'b1;
                    cache_we <= 1'b1;
                    cache_d <= 47'd0;
                    cache_addr <= clear_cache_counter;
                end
                
            end

            state_idle: 
            begin
                if (l2_addr < 27'h800000)
                begin
                    if ( ( (l2_start && !start_prev) || addr_prev >= 27'h800000 && l2_start) || start_registered)
                    begin
                        start_registered <= 1'b0;
                        if (l2_we)
                        begin
                            // update cache and write SDRAM
                            state <= state_writing;
                            sdc_addr_reg <= l2_addr;
                            sdc_we_reg <= 1'b1;
                            sdc_start_reg <= 1'b1;
                            sdc_data_reg <= l2_data;

                            cache_d <= {1'b1, l2_addr[23:index_size], l2_data}; // tag + data
                            cache_addr <= l2_addr[index_size-1:0];
                            
                        end
                        else
                        begin
                            // wait a cycle for cache to be read
                            cache_addr <= l2_addr[index_size-1:0];
                            state <= state_delay_cache;

                            // just in case we have a cache miss in the next cycle, prepare address on sdram controller bus
                            sdc_addr_reg <= l2_addr;
                            sdc_we_reg <= 1'b0;
                        end
                    end
                end
            end

            state_delay_cache:
            begin
                state <= state_check_cache;
            end

            state_writing:
            begin
                if (sdc_done)
                begin
                    state <= state_done_high;

                    sdc_addr_reg <= 24'd0;
                    sdc_we_reg <= 1'b0;
                    sdc_start_reg <= 1'b0;
                    sdc_data_reg <= 32'd0;

                    cache_we <= 1'b1;

                    l2_done_reg <= 1'b1;
                end
            end

            state_check_cache: 
            begin
                // check cache. if hit, return cached item
                if (cache_q[46] && sdc_addr_reg[23:index_size] == cache_q[45:32]) // valid and tag check
                begin
                    state <= state_done_high;

                    l2_done_reg <= 1'b1;
                    l2_q_reg <= cache_q[31:0];
                end
                // if miss, read from ram, place in cache, return cached item
                else
                begin
                    state <= state_miss_read_ram;

                    sdc_start_reg <= 1'b1;
                end
            end

            state_miss_read_ram: 
            begin
                if (sdc_done)
                begin
                    state <= state_done_high;

                    // we received item from ram, now write to cache and return
                    sdc_addr_reg <= 24'd0;
                    sdc_start_reg <= 1'b0;

                    cache_we <= 1'b1;
                    cache_d <= {1'b1, sdc_addr_reg[23:index_size], sdc_q}; // tag + data
                    

                    l2_done_reg <= 1'b1;
                    l2_q_reg <= sdc_q;
                end
            end

            state_done_high:
            begin
                // keep done high for one cycle as we run on double clock speed from CPU
                l2_done_reg <= 1'b1;
                state <= state_idle;
            end

        endcase
    end
end

// passthrough when above SDRAM memory range
assign sdc_addr =   (l2_addr < 27'h800000) ? sdc_addr_reg   : l2_addr;
assign sdc_data =   (l2_addr < 27'h800000) ? sdc_data_reg   : l2_data;
assign sdc_we =     (l2_addr < 27'h800000) ? sdc_we_reg     : l2_we;
assign sdc_start =  (l2_addr < 27'h800000) ? sdc_start_reg  : l2_start;
assign l2_q =       (l2_addr < 27'h800000) ? l2_q_reg       : sdc_q;
assign l2_done =    (l2_addr < 27'h800000) ? l2_done_reg    : sdc_done;

endmodule