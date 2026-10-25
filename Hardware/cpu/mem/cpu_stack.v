// Hardware stack
// 256 words in a block RAM, and the stack pointer SP. A push writes at SP and then
// increments it, a pop decrements SP and then gives the word there. SP wraps around.
module cpu_stack (
    input  wire        clk,
    input  wire        reset,

    // What happens at this clock edge, at most one of them
    input  wire        push,
    input  wire [31:0] push_value,
    input  wire        pop,
    input  wire        sp_write,       // a write to SP, from the CPU-internal I/O
    input  wire [7:0]  sp_write_value,

    output reg  [7:0]  sp,
    output reg  [31:0] top,            // the word a pop gets, see below
    output wire        overflow,       // a push when SP is 255
    output wire        underflow       // a pop when SP is 0
);

// The value of SP after this clock edge
wire [7:0] next_sp = reset    ? 8'd0           :
                     sp_write ? sp_write_value :
                     push     ? sp + 8'd1      :
                     pop      ? sp - 8'd1      :
                                sp;

// The block RAM reads the top of the stack as it is after this clock edge, so top is
// always the word below SP. The one exception is the cycle after a push: the push
// writes that same word at the same edge, and the block RAM then gives its old value.
reg [31:0] ram [0:255];

// The block RAM starts at 0, like the stack of the ISS.
integer i;
initial for (i = 0; i < 256; i = i + 1) ram[i] = 32'd0;

always @(posedge clk) begin
    if (push)
        ram[sp] <= push_value;
    top <= ram[next_sp - 8'd1];
end

always @(posedge clk)
    sp <= next_sp;

assign overflow  = push && sp == 8'd255;
assign underflow = pop && sp == 8'd0;

endmodule
