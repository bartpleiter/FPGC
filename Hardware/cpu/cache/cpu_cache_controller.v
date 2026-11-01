// Cache controller
// Fills lines of the L1I and the L1D from SDRAM, and first writes a dirty L1D line back
// that a fill replaces. It handles one line at a time, and takes CCACHE and CCACHED and a
// miss of the L1D before a miss of the L1I. It streams the words between the SDRAM line
// port and port B of the caches, one per cycle, so it never holds a whole line. Every
// output is a register, apart from the write-back word, which comes straight from port B
// of the L1D. See l1-cache.md, "The cache controller", "CCACHE, CCACHED and reset" and
// "The SDRAM line port".
//
// A stage with a miss sets its request and keeps it until the controller takes it. From
// the take until after the last write to that stage's cache, the cache's writing signal
// is 1. The stage then reads its cache again.
//
// During a fill, the controller says which word of which line comes next on the SDRAM
// port, so that a stage that waits for that word can use it in the cycle it arrives
// (early restart). The fill starts at the word the stage waits for.
//
// CCACHE and CCACHED walk through the lines: each dirty line of the L1D is written back,
// and every entry of the L1D, and for CCACHE of the L1I, is written 0, which makes the
// line invalid. After a reset, the walk only writes the entries of both caches 0, one
// line per cycle, without writing anything back.
module cpu_cache_controller #(
    parameter L1I_LINES = 128,  // the number of lines of each cache, a power of two
    parameter L1D_LINES = 128
) (
    input  wire                              clk,
    input  wire                              reset,

    // A miss in IF. The address is the word that IF waits for, and the fill starts there.
    input  wire                              l1i_request,
    input  wire [25:2]                       l1i_addr,
    output reg                               l1i_take,
    output reg                               l1i_writing,

    // A miss in MEM, with the line it replaces: whether that line is dirty, and its tag
    input  wire                              l1d_request,
    input  wire [25:2]                       l1d_addr,
    input  wire                              victim_dirty,
    input  wire [20-$clog2(L1D_LINES):0]     victim_tag,
    output reg                               l1d_take,
    output reg                               l1d_writing,

    // The controller fills a line of the L1I or the L1D, and the word that arrives next
    // with sdram_read_valid is the one at fill_addr. The flags are 1 from the request
    // of the fill on the SDRAM port until after its last word.
    output reg                               l1i_filling,
    output reg                               l1d_filling,
    output wire [25:2]                       fill_addr,

    // CCACHE (clear_data_only 0) or CCACHED (1) from MEM. MEM keeps the request until
    // clear_take, and the walk is done when l1d_writing is 0 again. For CCACHE,
    // l1i_writing ends together with it.
    input  wire                              clear_request,
    input  wire                              clear_data_only,
    output reg                               clear_take,

    // Port B of the caches. The word a fill writes goes to both, with a write enable
    // and an address for each.
    output reg  [31:0]                       b_write_data,
    output reg  [$clog2(L1I_LINES)+4:2]      l1i_b_addr,
    output reg                               l1i_b_write,
    output reg                               l1i_b_entry_write,
    output reg  [21-$clog2(L1I_LINES):0]     l1i_b_entry_write_value,
    output reg  [$clog2(L1D_LINES)+4:2]      l1d_b_addr,
    input  wire [31:0]                       l1d_b_word,
    input  wire [22-$clog2(L1D_LINES):0]     l1d_b_entry,
    output reg                               l1d_b_write,
    output reg                               l1d_b_entry_write,
    output reg  [22-$clog2(L1D_LINES):0]     l1d_b_entry_write_value,

    // The SDRAM line port. A request stays 1 until sdram_take. A read gives the 8 words
    // of the line with sdram_read_valid, starting at word sdram_first and wrapping
    // around. A write asks for each word with sdram_next, and takes sdram_write_word in
    // the cycle after.
    output reg                               sdram_req,
    output reg                               sdram_write,
    output reg  [25:5]                       sdram_line,
    output reg  [2:0]                        sdram_first,
    input  wire                              sdram_take,
    input  wire                              sdram_read_valid,
    input  wire [31:0]                       sdram_read_word,
    input  wire                              sdram_next,
    output wire [31:0]                       sdram_write_word
);

localparam L1I_INDEX = $clog2(L1I_LINES);
localparam L1D_INDEX = $clog2(L1D_LINES);

// The walk goes through the lines of the larger cache.
localparam WALK_LINES = L1I_LINES > L1D_LINES ? L1I_LINES : L1D_LINES;
localparam WALK_INDEX = $clog2(WALK_LINES);

localparam IDLE       = 2'd0;  // waits for a request
localparam WRITE_BACK = 2'd1;  // sends the dirty line of the L1D to SDRAM
localparam FILL       = 2'd2;  // writes the line from SDRAM into the cache
localparam WALK       = 2'd3;  // invalidates every line, and writes dirty lines back

reg [1:0]  state;
reg        fill_l1d;  // the line goes into the L1D, else into the L1I
reg [25:5] line;      // the line to fill
reg [2:0]  first;     // the word of the line the fill starts at
reg [2:0]  word;      // the word of the line that arrives next
reg [2:0]  count;     // the words of the fill that have arrived

reg [WALK_INDEX-1:0] index;  // the line of the walk
reg        clearing;         // a walk of CCACHE or CCACHED is running
reg        walk_l1i;         // the walk invalidates the L1I too
reg        entry_read;       // port B of the L1D has read the entry of the line

// The line of the walk as a number, whether each cache has it, and whether it's the
// last line of the walk. The walk of CCACHED only goes through the lines of the L1D.
wire [31:0] walk_line  = {{(32-WALK_INDEX){1'b0}}, index};
wire        walk_l1i_has = walk_line < L1I_LINES;
wire        walk_l1d_has = walk_line < L1D_LINES;
wire        walk_last    = walk_line == (walk_l1i ? WALK_LINES : L1D_LINES) - 1;
wire        walk_dirty   = l1d_b_entry[22-L1D_INDEX] && l1d_b_entry[21-L1D_INDEX];

// Port B of the L1D reads at l1d_b_addr at every clock edge. During a write-back that's
// the word the SDRAM port asks for next, so the word is there in the cycle after
// sdram_next.
assign sdram_write_word = l1d_b_word;

assign fill_addr = {line, word};

always @(posedge clk) begin
    l1i_take          <= 1'b0;
    l1d_take          <= 1'b0;
    clear_take        <= 1'b0;
    l1i_b_write       <= 1'b0;
    l1d_b_write       <= 1'b0;
    l1i_b_entry_write <= 1'b0;
    l1d_b_entry_write <= 1'b0;
    if (sdram_take)
        sdram_req <= 1'b0;

    // A reset starts the walk that invalidates both caches.
    if (reset) begin
        state                   <= WALK;
        clearing                <= 1'b0;
        walk_l1i                <= 1'b1;
        index                   <= {WALK_INDEX{1'b0}};
        entry_read              <= 1'b0;
        l1i_b_addr              <= {(L1I_INDEX+3){1'b0}};
        l1d_b_addr              <= {(L1D_INDEX+3){1'b0}};
        l1i_b_entry_write_value <= {(22-L1I_INDEX){1'b0}};
        l1d_b_entry_write_value <= {(23-L1D_INDEX){1'b0}};
        l1i_writing             <= 1'b1;
        l1d_writing             <= 1'b1;
        l1i_filling             <= 1'b0;
        l1d_filling             <= 1'b0;
        sdram_req               <= 1'b0;
    end else begin
        case (state)
            IDLE: begin
                count <= 3'd0;
                if (clear_request) begin
                    clear_take              <= 1'b1;
                    l1d_writing             <= 1'b1;
                    l1i_writing             <= !clear_data_only;
                    state                   <= WALK;
                    clearing                <= 1'b1;
                    walk_l1i                <= !clear_data_only;
                    index                   <= {WALK_INDEX{1'b0}};
                    entry_read              <= 1'b0;
                    l1i_b_addr              <= {(L1I_INDEX+3){1'b0}};
                    l1d_b_addr              <= {(L1D_INDEX+3){1'b0}};
                    l1i_b_entry_write_value <= {(22-L1I_INDEX){1'b0}};
                    l1d_b_entry_write_value <= {(23-L1D_INDEX){1'b0}};
                end else if (l1d_request) begin
                    l1d_take    <= 1'b1;
                    l1d_writing <= 1'b1;
                    fill_l1d    <= 1'b1;
                    line        <= l1d_addr[25:5];
                    first       <= l1d_addr[4:2];
                    word        <= l1d_addr[4:2];
                    sdram_req   <= 1'b1;
                    l1d_filling <= !victim_dirty;
                    if (victim_dirty) begin
                        state       <= WRITE_BACK;
                        sdram_write <= 1'b1;
                        sdram_line  <= {victim_tag, l1d_addr[L1D_INDEX+4:5]};
                        sdram_first <= 3'd0;
                        l1d_b_addr  <= {l1d_addr[L1D_INDEX+4:5], 3'd0};
                    end else begin
                        state       <= FILL;
                        sdram_write <= 1'b0;
                        sdram_line  <= l1d_addr[25:5];
                        sdram_first <= l1d_addr[4:2];
                    end
                end else if (l1i_request) begin
                    l1i_take    <= 1'b1;
                    l1i_writing <= 1'b1;
                    fill_l1d    <= 1'b0;
                    line        <= l1i_addr[25:5];
                    first       <= l1i_addr[4:2];
                    word        <= l1i_addr[4:2];
                    state       <= FILL;
                    l1i_filling <= 1'b1;
                    sdram_req   <= 1'b1;
                    sdram_write <= 1'b0;
                    sdram_line  <= l1i_addr[25:5];
                    sdram_first <= l1i_addr[4:2];
                end
            end

            // The SDRAM port takes the word of each sdram_next in the cycle after it.
            // After the last one, the fill of the new line is requested, or the walk
            // goes on.
            WRITE_BACK: begin
                if (sdram_next) begin
                    l1d_b_addr[4:2] <= l1d_b_addr[4:2] + 3'd1;
                    if (l1d_b_addr[4:2] == 3'd7) begin
                        if (clearing) begin
                            state <= WALK;
                        end else begin
                            state       <= FILL;
                            l1d_filling <= 1'b1;
                            sdram_req   <= 1'b1;
                            sdram_write <= 1'b0;
                            sdram_line  <= line;
                            sdram_first <= first;
                        end
                    end
                end
            end

            // Each word is written in the cycle after it arrives. The words arrive from
            // word first on, and wrap around. The entry {valid, clean, tag} is written
            // with the last word, and the fill ends in the cycle of that write.
            FILL: begin
                if (l1i_b_entry_write || l1d_b_entry_write) begin
                    state       <= IDLE;
                    l1i_writing <= 1'b0;
                    l1d_writing <= 1'b0;
                end else if (sdram_read_valid) begin
                    count        <= count + 3'd1;
                    word         <= word + 3'd1;
                    b_write_data <= sdram_read_word;
                    l1i_b_addr   <= {line[L1I_INDEX+4:5], word};
                    l1d_b_addr   <= {line[L1D_INDEX+4:5], word};
                    l1i_b_write  <= !fill_l1d;
                    l1d_b_write  <= fill_l1d;
                    if (count == 3'd7) begin
                        l1i_filling             <= 1'b0;
                        l1d_filling             <= 1'b0;
                        l1i_b_entry_write       <= !fill_l1d;
                        l1d_b_entry_write       <= fill_l1d;
                        l1i_b_entry_write_value <= {1'b1, line[25:L1I_INDEX+5]};
                        l1d_b_entry_write_value <= {2'b10, line[25:L1D_INDEX+5]};
                    end
                end
            end

            // The walk of CCACHE and CCACHED takes two cycles per line. In the first,
            // port B of the L1D reads the entry. In the second, the entry is there,
            // and the entries of the line are written 0 at the end of it. If the line
            // was dirty, it's written back from there, and the walk then comes back to
            // the same line, whose entry now reads 0, and moves on. The walk after a
            // reset doesn't need the entries, so after the first line it writes one
            // line per cycle. The walk ends in the cycle of its last write, as a fill
            // does.
            WALK: begin
                if (!entry_read) begin
                    entry_read        <= 1'b1;
                    l1i_b_entry_write <= walk_l1i && walk_l1i_has;
                    l1d_b_entry_write <= walk_l1d_has;
                end else if (clearing && walk_l1d_has && walk_dirty) begin
                    state       <= WRITE_BACK;
                    sdram_req   <= 1'b1;
                    sdram_write <= 1'b1;
                    sdram_line  <= {l1d_b_entry[20-L1D_INDEX:0], index[L1D_INDEX-1:0]};
                    sdram_first <= 3'd0;
                end else if (walk_last) begin
                    state       <= IDLE;
                    clearing    <= 1'b0;
                    l1i_writing <= 1'b0;
                    l1d_writing <= 1'b0;
                end else begin
                    index      <= index + 1'b1;
                    l1i_b_addr <= {index[L1I_INDEX-1:0] + 1'b1, 3'd0};
                    l1d_b_addr <= {index[L1D_INDEX-1:0] + 1'b1, 3'd0};
                    entry_read <= !clearing;
                    if (!clearing) begin
                        l1i_b_entry_write <= walk_line + 1 < L1I_LINES;
                        l1d_b_entry_write <= walk_line + 1 < L1D_LINES;
                    end
                end
            end
        endcase
    end
end

`ifndef SYNTHESIS
// Checks in simulation of what the controller promises at its ports. It takes a request
// only when it isn't busy with another one, and only a request that is there. A fill
// writes each of the 8 words of its line once, and the entry together with the last
// word. The cache's writing signal is 1 at every write, and stays 1 until the entry is
// written. A walk writes no words, writes every entry it invalidates once with 0, and
// ends l1i_writing no later than l1d_writing. The walk after a reset doesn't use the
// SDRAM port. A word from SDRAM only arrives for a fill, of one of the caches.
reg       was_writing;      // either writing signal, in the cycle before
reg       was_l1i_writing;
reg       was_l1d_writing;
reg       had_l1i_request;  // the requests, in the cycle before
reg       had_l1d_request;
reg       had_clear_request;
reg [7:0] l1i_words;        // the words of the line written since the take
reg [7:0] l1d_words;
reg       l1i_filled;       // the entry is written since the take
reg       l1d_filled;
reg       walking;          // from the take of a clear, or a reset, until the walk ends
reg       reset_walk;       // the walk is the one after a reset
reg       walk_clears_l1i;  // the walk invalidates the L1I
integer   l1i_cleared;      // the entries the walk has written
integer   l1d_cleared;

wire [7:0] l1i_word_bit = 8'd1 << l1i_b_addr[4:2];
wire [7:0] l1d_word_bit = 8'd1 << l1d_b_addr[4:2];
wire       walk_ends    = walking && was_l1d_writing && !l1d_writing;

always @(posedge clk) begin
    if (!reset) begin
        if ((l1i_take || l1d_take || clear_take) && was_writing)
            $fatal(1, "cpu_cache_controller took a request while busy");
        if ((l1i_take && !had_l1i_request) || (l1d_take && !had_l1d_request) ||
            (clear_take && !had_clear_request))
            $fatal(1, "cpu_cache_controller took a request that wasn't there");
        if ((l1i_b_write || l1i_b_entry_write) && !l1i_writing)
            $fatal(1, "cpu_cache_controller wrote the L1I without l1i_writing");
        if ((l1d_b_write || l1d_b_entry_write) && !l1d_writing)
            $fatal(1, "cpu_cache_controller wrote the L1D without l1d_writing");
        if ((l1i_b_write && (l1i_words & l1i_word_bit) != 8'd0) ||
            (l1d_b_write && (l1d_words & l1d_word_bit) != 8'd0))
            $fatal(1, "cpu_cache_controller wrote a word of a line twice");
        if ((l1i_b_entry_write && l1i_b_entry_write_value[21-L1I_INDEX] &&
             (!l1i_b_write || (l1i_words | l1i_word_bit) != 8'hFF)) ||
            (l1d_b_entry_write && l1d_b_entry_write_value[22-L1D_INDEX] &&
             (!l1d_b_write || (l1d_words | l1d_word_bit) != 8'hFF)))
            $fatal(1, "cpu_cache_controller wrote an entry before the whole line");
        if (!walking && ((was_l1i_writing && !l1i_writing && !l1i_filled) ||
                         (was_l1d_writing && !l1d_writing && !l1d_filled)))
            $fatal(1, "cpu_cache_controller ended a fill before the entry was written");
        if (walking && (l1i_b_write || l1d_b_write))
            $fatal(1, "cpu_cache_controller wrote a word during a walk");
        if (walking &&
            ((l1i_b_entry_write && l1i_b_entry_write_value != {(22-L1I_INDEX){1'b0}}) ||
             (l1d_b_entry_write && l1d_b_entry_write_value != {(23-L1D_INDEX){1'b0}})))
            $fatal(1, "cpu_cache_controller wrote a valid entry during a walk");
        if (reset_walk && sdram_req)
            $fatal(1, "cpu_cache_controller used the SDRAM port in the walk after reset");
        if (sdram_read_valid && !l1i_filling && !l1d_filling)
            $fatal(1, "cpu_cache_controller got a word from SDRAM outside a fill");
        if (walk_ends && (l1i_writing || l1d_cleared != L1D_LINES ||
                          l1i_cleared != (walk_clears_l1i ? L1I_LINES : 0)))
            $fatal(1, "cpu_cache_controller ended a walk before every entry was written");
    end
    was_writing       <= (l1i_writing || l1d_writing) && !reset;
    was_l1i_writing   <= l1i_writing && !reset;
    was_l1d_writing   <= l1d_writing && !reset;
    had_l1i_request   <= l1i_request;
    had_l1d_request   <= l1d_request;
    had_clear_request <= clear_request;
    if (reset || l1i_take) begin
        l1i_words  <= 8'd0;
        l1i_filled <= 1'b0;
    end else begin
        if (l1i_b_write)
            l1i_words <= l1i_words | l1i_word_bit;
        if (l1i_b_entry_write)
            l1i_filled <= 1'b1;
    end
    if (reset || l1d_take) begin
        l1d_words  <= 8'd0;
        l1d_filled <= 1'b0;
    end else begin
        if (l1d_b_write)
            l1d_words <= l1d_words | l1d_word_bit;
        if (l1d_b_entry_write)
            l1d_filled <= 1'b1;
    end
    if (reset || clear_take) begin
        walking         <= 1'b1;
        reset_walk      <= reset;
        walk_clears_l1i <= reset || !clear_data_only;
        l1i_cleared     <= 0;
        l1d_cleared     <= 0;
    end else begin
        if (walk_ends) begin
            walking    <= 1'b0;
            reset_walk <= 1'b0;
        end
        if (walking && l1i_b_entry_write)
            l1i_cleared <= l1i_cleared + 1;
        if (walking && l1d_b_entry_write)
            l1d_cleared <= l1d_cleared + 1;
    end
end
`endif

endmodule
