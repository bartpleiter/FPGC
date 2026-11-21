// FIFO
module fifo #(
    parameter WIDTH      = 8,
    parameter DEPTH_BITS = 5
) (
    input  wire             clk,
    input  wire             reset,

    input  wire             push,
    input  wire [WIDTH-1:0] write_data,
    input  wire             pop,
    output reg  [WIDTH-1:0] read_data,

    output wire             empty,
    output wire             full
);

localparam DEPTH = 1 << DEPTH_BITS;

reg [WIDTH-1:0] words [0:DEPTH-1];

// One bit more than the address, so full and empty differ
reg [DEPTH_BITS:0] write_ptr;
reg [DEPTH_BITS:0] read_ptr;

assign empty = write_ptr == read_ptr;
assign full  = write_ptr == {!read_ptr[DEPTH_BITS], read_ptr[DEPTH_BITS-1:0]};

wire do_push = push && !full;
wire do_pop  = pop && !empty;

always @(posedge clk) begin
    if (do_push)
        words[write_ptr[DEPTH_BITS-1:0]] <= write_data;
    if (do_pop)
        read_data <= words[read_ptr[DEPTH_BITS-1:0]];
end

always @(posedge clk) begin
    if (reset) begin
        write_ptr <= {(DEPTH_BITS+1){1'b0}};
        read_ptr  <= {(DEPTH_BITS+1){1'b0}};
    end else begin
        if (do_push)
            write_ptr <= write_ptr + 1'b1;
        if (do_pop)
            read_ptr <= read_ptr + 1'b1;
    end
end

endmodule
