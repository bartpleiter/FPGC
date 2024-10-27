/*
* Instruction Memory
*/

module InstrMem(
    input clk,
    input reset,
    input [31:0] addr,
    output hit,
    output [31:0] q,

    // bus
    output [31:0] bus_addr,
    output [31:0] bus_data,
    output        bus_we,
    output reg    bus_start,
    input [31:0]  bus_q,
    input         bus_done,
    input         bus_ready,

    input clear, hold
);

// in case of a clear mid transaction, ignore the next result and start again
reg ignoreNext = 1'b0;
reg [31:0] addr_prev = 32'd0;
reg hit_prev = 1'b0;
reg hold_reg = 1'b0;

assign hit = (bus_done && !ignoreNext && !clear) || (hold_reg && hit_prev);
assign q =  (hit_prev) ? bus_q : 32'd0;

assign bus_addr = addr;
assign bus_data = 32'd0;
assign bus_we = 1'b0;

reg bus_start_prev = 1'b0;

always @(posedge clk) 
begin
    if (reset)
        ignoreNext <= 1'b0;
    else if (ignoreNext && (bus_done || bus_ready))
        ignoreNext <= 1'b0;
    else if (clear && (bus_start || !bus_ready))
        ignoreNext <= 1'b1;
end

always @(posedge clk)
begin
    if (reset)
    begin
        bus_start <= 0;
        bus_start_prev <= 0;
        addr_prev <= 32'd0;
        hit_prev <= 1'b0;
        hold_reg <= 1'b0;
    end
    else
    begin
        if (!hold && !clear && ((bus_done || hit) || addr == 0 || ignoreNext))
        begin
            bus_start <= 1'b1;
        end
        else
            bus_start <= 1'b0;

        addr_prev <= addr;
        bus_start_prev <= bus_start;
        hit_prev <= hit;

        if (hold)
            hold_reg <= 1'b1;
        else
            hold_reg <= 1'b0;
    end
end

endmodule