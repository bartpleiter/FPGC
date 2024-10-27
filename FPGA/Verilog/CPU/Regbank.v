/*
* Register Bank
*/

module Regbank(
    input               clk, reset,

    input       [3:0]   addr_a, addr_b,
    output      [31:0]  data_a, data_b, 

    input       [3:0]   addr_d,
    input       [31:0]  data_d,
    input               we, clear, hold
);

reg [31:0] regs [0:15]; // 16 registers of 32 bit, although reg0 is unused


reg [31:0] ramResulta = 32'd0;
reg [31:0] ramResultb = 32'd0;

// RAM logic
always @(posedge clk) 
begin
    ramResulta <= regs[addr_a];
    ramResultb <= regs[addr_b];

    if (we && addr_d != 4'd0)
    begin
        regs[addr_d] <= data_d;
        //$display("%d: reg%d := %d", $time, addr_d, data_d);
    end
end

reg [31:0] data_a_reg = 32'd0;
reg [31:0] data_b_reg = 32'd0;

reg useRamResult_a = 1'b0;
reg useRamResult_b = 1'b0;

assign data_a = (useRamResult_a) ? ramResulta : data_a_reg;
assign data_b = (useRamResult_b) ? ramResultb : data_b_reg;

// Read
always @(posedge clk) 
begin
    if (reset)
    begin
        useRamResult_a <= 1'b0;
        useRamResult_b <= 1'b0;
        data_a_reg <= 32'd0;
        data_b_reg <= 32'd0;
    end
    else
    begin
        useRamResult_a <= 1'b0;
        useRamResult_b <= 1'b0;

        if (clear)
        begin
            data_a_reg <= 32'd0;
        end
        else if (hold)
        begin
            data_a_reg <= data_a_reg;
        end
        else if (addr_a == 4'd0)
        begin
            data_a_reg <= 32'd0;
        end
        else if ((addr_a == addr_d) && we)
        begin
            data_a_reg <= data_d;
        end
        else
        begin
            //data_a <= regs[addr_a];
            useRamResult_a <= 1'b1;
        end

        if (clear)
        begin
            data_b_reg <= 32'd0;
        end
        else if (hold)
        begin
            data_b_reg <= data_b_reg;
        end
        else if (addr_b == 4'd0)
        begin
            data_b_reg <= 32'd0;
        end
        else if ((addr_b == addr_d) && we)
        begin
            data_b_reg <= data_d;
        end
        else
        begin
            //data_b <= regs[addr_b];
            useRamResult_b <= 1'b1;
        end
    end
end

integer i;
initial
begin
    for (i = 0; i < 16; i = i + 1)
    begin
        regs[i] = 32'd0;
    end
end

endmodule