/*
* Data Memory
*/

module DataMem(
    input wire          clk, reset,
    input wire  [31:0]  addr,
    input wire          we,
    input wire          re,
    input wire  [31:0]  data,
    output wire [31:0]  q,
    output              busy,

    // bus
    output [31:0] bus_addr,
    output [31:0] bus_data,
    output        bus_we,
    output        bus_start,
    input [31:0]  bus_q,
    input         bus_done,
    input         bus_ready,

    input wire          clear, hold
);

reg [31:0] qreg = 32'd0;
reg busy_reg = 1'b0;

wire read_or_write = we || re;
reg read_or_write_prev = 1'b0;
wire read_or_write_edge = read_or_write && !read_or_write_prev;

assign bus_addr = addr;
assign bus_data = data;
assign bus_we = we;
assign bus_start = read_or_write_edge;
assign busy = read_or_write && !bus_done;
assign q = (bus_done) ? bus_q : qreg;



// Clear and Hold are currently skipped because they are not used in the CPU

always @(posedge clk)
begin
    if (reset)
    begin
        read_or_write_prev <= 1'b0;
        qreg <= 32'd0;
    end
    else
    begin
        read_or_write_prev <= read_or_write;
        if (bus_done)
        begin
            qreg <= bus_q;
        end
    end
end


endmodule