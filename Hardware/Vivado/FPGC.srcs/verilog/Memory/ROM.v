/*
* Internal FPGA ROM
* Implemented as dual port rom for simultaneous
* access by the Fetch and Memory stages of the CPU
* Contains output enable signals for both ports
* Additionally, it contains a hold signal for the Fetch stage
*/
module ROM #(
    parameter WIDTH = 32,
    parameter WORDS = 512,
    parameter ADDR_BITS = 9,
    parameter LIST = "memory/rom.list"
) (
    input  wire                 clk,
    input  wire                 reset,

    input  wire [ADDR_BITS-1:0] fe_addr,
    input  wire                 fe_oe,
    output reg  [    WIDTH-1:0] fe_q,
    input  wire                 fe_hold,

    input  wire [ADDR_BITS-1:0] mem_addr,
    output reg  [    WIDTH-1:0] mem_q
);

reg [WIDTH-1:0] rom[0:WORDS-1];

always @(posedge clk) 
begin
    if (fe_hold)
    begin
        fe_q <= fe_q;
    end
    else
    begin
        if (fe_oe)
        begin
            fe_q <= rom[fe_addr];
        end
        else
        begin
            fe_q <= {WIDTH{1'b0}};
        end
    end

    mem_q <= rom[mem_addr];

end

initial
begin
    $readmemb(LIST, rom);
    mem_q = {WIDTH{1'b0}};
end

endmodule
