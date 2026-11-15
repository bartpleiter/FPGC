// Hardware stack
module cpu_stack (
    input  wire        clk,
    input  wire        reset,

    input  wire        push,
    input  wire [31:0] push_value,
    input  wire        pop,
    input  wire        sp_write,
    input  wire [7:0]  sp_write_value,

    output reg  [7:0]  sp,
    output reg  [31:0] top,
    output wire        overflow,
    output wire        underflow
);

// The value of SP after this clock edge
wire [7:0] next_sp = reset    ? 8'd0           :
                     sp_write ? sp_write_value :
                     push     ? sp + 8'd1      :
                     pop      ? sp - 8'd1      :
                                sp;

reg [31:0] ram [0:255];

// Only needed in simulation as Quartus initializes the RAMs to 0 by itself
integer i;

`ifndef SYNTHESIS
initial for (i = 0; i < 256; i = i + 1) ram[i] = 32'd0;
`endif

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
