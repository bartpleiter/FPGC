/*
 * Stack
 * 32 Bits wide, 64 words long
 * Supports push, pop, and direct pointer read/write
 *
 * Uses M9K block RAM with a read-ahead scheme: the BRAM read address
 * is kept at (ptr - 1) so the top-of-stack value is always available
 * without added latency.  A forwarding register handles the case where
 * a push is immediately followed by a pop (same-address write-then-read
 * conflict in simple dual-port BRAM).
 */
module Stack (
    input wire          clk,
    input wire          reset,

    input wire  [31:0]  d,
    output wire [31:0]  q,
    input wire          push,
    input wire          pop,

    input wire          clear,

    // Pointer access
    output wire [5:0]   ptr_out,
    input wire  [5:0]   ptr_in,
    input wire          ptr_we
);

reg [5:0]   ptr = 6'd0;    // Stack pointer (0-63)

// ---- BRAM (simple dual-port: port A = read, port B = write) ----
reg [31:0] stack_mem [63:0];
reg [5:0]  read_addr = 6'd0;   // Registered read address (read-ahead)
reg [31:0] bram_q = 32'd0;     // BRAM read output (1-cycle latency)

// BRAM read port (synchronous)
always @(posedge clk)
    bram_q <= stack_mem[read_addr];

// BRAM write port (synchronous)
always @(posedge clk)
    if (push)
        stack_mem[ptr] <= d;

// ---- Forwarding for push-then-pop ----
// When a push writes to addr X and the next cycle reads addr X,
// the BRAM may return stale data.  Forward the pushed value instead.
reg [31:0] fwd_data = 32'd0;
reg        fwd_valid = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        fwd_data <= 32'd0;
        fwd_valid <= 1'b0;
    end
    else
    begin
        fwd_data <= d;
        fwd_valid <= push;
    end
end

wire [31:0] stack_top = fwd_valid ? fwd_data : bram_q;

// ---- Output register ----
reg [31:0] q_reg = 32'd0;
assign q = q_reg;
assign ptr_out = ptr;

// ---- Control logic ----
always @(posedge clk)
begin
    if (reset)
    begin
        ptr <= 6'd0;
        q_reg <= 32'd0;
        read_addr <= 6'd0;
    end
    else if (clear)
    begin
        q_reg <= 32'd0;
    end
    else if (ptr_we)
    begin
        ptr <= ptr_in;
        read_addr <= ptr_in - 1'b1;
    end
    else
    begin
        if (push)
        begin
            ptr <= ptr + 1'b1;
            // After push: new top-of-stack is at (ptr+1)-1 = ptr
            read_addr <= ptr;
        end

        if (pop)
        begin
            q_reg <= stack_top;
            ptr <= ptr - 1'b1;
            // After pop: new top-of-stack is at (ptr-1)-1 = ptr-2
            read_addr <= ptr - 2'd2;
        end
    end
end

integer i;
initial
begin
    for (i = 0; i < 64; i = i + 1)
    begin
        stack_mem[i] = 32'd0;
    end
end

endmodule
