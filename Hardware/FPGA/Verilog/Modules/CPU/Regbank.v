/*
 * Regbank
 * Contains 16 registers of 32 bit each
 * Designed to be inferred as block RAM
 * Reg0 is always 0
 * TODO: this can be simplified a lot as we have enough logic elements to store all registers in logic.
 * This would allow reading all registers by a potential debugger more easily.
 */
module Regbank (
    // Clock and reset
    input wire clk,
    input wire reset,

    // REG stage ports
    input wire  [3:0]   addr_a,
    input wire  [3:0]   addr_b,
    input wire          clear,
    input wire          hold,
    output wire [31:0]  data_a,
    output wire [31:0]  data_b,

    // WB stage ports
    input wire  [3:0]   addr_d,
    input wire  [31:0]  data_d,
    input wire          we
);

wire we_no_zero = (we && (addr_d != 4'd0));

reg [31:0] regs [0:15];

// RamResult are the regs that are read from block RAM
// Depending on other signals, a different value should be returned
reg [31:0] ramResulta = 32'd0;
reg [31:0] ramResultb = 32'd0;

// RAM logic
always @(posedge clk) 
begin
    if (reset)
    begin
        ramResulta <= 32'd0;
        ramResultb <= 32'd0;
    end
    else
    begin
        if (hold)
        begin
            ramResulta <= data_a;
            ramResultb <= data_b;
        end
        else
        begin
            ramResulta <= regs[addr_a];
            ramResultb <= regs[addr_b];
        end

        if (we_no_zero)
        begin
            regs[addr_d] <= data_d;

            $display("%0t reg r%02d: %d", $time, addr_d, data_d);
        end
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
            // Return 0 on clear
            data_a_reg <= 32'd0;
        end
        else if (hold)
        begin
            // Hold is now in RAM logic
            useRamResult_a <= 1'b1;
        end
        else if (addr_a == 4'd0)
        begin
            // Return 0 on reg0
            data_a_reg <= 32'd0;
        end
        else if ((addr_a == addr_d) && we)
        begin
            // Return new value on write from WB
            data_a_reg <= data_d;
        end
        else
        begin
            // Use block RAM result otherwise
            useRamResult_a <= 1'b1;
            data_a_reg <= ramResulta;
        end

        if (clear)
        begin
            // Return 0 on clear
            data_b_reg <= 32'd0;
        end
        else if (hold)
        begin
            // Hold is now in RAM logic
            useRamResult_b <= 1'b1;
        end
        else if (addr_b == 4'd0)
        begin
            // Return 0 on reg0
            data_b_reg <= 32'd0;
        end
        else if ((addr_b == addr_d) && we)
        begin
            // Return new value on write from WB
            data_b_reg <= data_d;
        end
        else
        begin
            // Use block RAM result otherwise
            useRamResult_b <= 1'b1;
            data_b_reg <= ramResultb;
        end
    end
end

// Initialize all registers to be 0
integer i;
initial
begin
    for (i = 0; i < 16; i = i + 1)
    begin
        regs[i] = 32'd0;
    end
end

endmodule
