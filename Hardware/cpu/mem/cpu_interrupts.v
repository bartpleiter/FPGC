// Interrupt controller
// A rising edge on an interrupt input makes that interrupt pending, and it stays pending
// until the CPU takes it. The lowest pending number goes first.
module cpu_interrupts #(
    parameter INTERRUPTS = 7  // the number of inputs, at most 255
) (
    input  wire                clk,
    input  wire                reset,
    input  wire [INTERRUPTS:1] interrupts,  // the inputs, bit n for interrupt n
    input  wire                take,        // the CPU takes interrupt `id` at this clock edge
    output wire                pending,     // an interrupt is pending
    output reg  [7:0]          id           // the lowest pending interrupt, or 0
);

// The inputs can come from another clock domain, so each one goes through two flip-flops
// first, to give a value that is about to change time to settle (a synchronizer). The
// third flip-flop holds the value of the cycle before, for the edge detection.
reg [INTERRUPTS:1] sync1;
reg [INTERRUPTS:1] sync2;
reg [INTERRUPTS:1] last;
reg [INTERRUPTS:1] pending_bits;

always @(posedge clk) begin
    sync1 <= interrupts;
    sync2 <= sync1;
    last  <= sync2;
end

wire [INTERRUPTS:1] rising = sync2 & ~last;

assign pending = pending_bits != {INTERRUPTS{1'b0}};

// The loop goes from the highest number down, so a lower pending number replaces a
// higher one.
integer n;

always @(*) begin
    id = 8'd0;
    for (n = INTERRUPTS; n >= 1; n = n - 1)
        if (pending_bits[n])
            id = n[7:0];
end

// A new edge in the cycle that its interrupt is taken makes it pending again.
always @(posedge clk) begin
    if (reset)
        pending_bits <= {INTERRUPTS{1'b0}};
    else
        for (n = 1; n <= INTERRUPTS; n = n + 1)
            if (rising[n])
                pending_bits[n] <= 1'b1;
            else if (take && id == n[7:0])
                pending_bits[n] <= 1'b0;
end

endmodule
