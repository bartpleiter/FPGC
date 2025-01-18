/*
* Internal FPGA ROM
* Dual port so the instruction memory and data memory can access it at the same time
* 512 addresses at 32 bits = 2KiB
*/
module ROM(
    input wire clk,                 // Clock signal
    input wire [8:0] addr_instr,    // 9-bit address for port A (2^9 = 512 addresses)
    input wire [8:0] addr_data,     // 9-bit address for port B (2^9 = 512 addresses)
    output reg [31:0] q_instr,      // 32-bit data output for port A
    output reg [31:0] q_data        // 32-bit data output for port B
);

    // ROM memory array: 512 locations, each 32 bits wide
    reg [31:0] rom [0:511];

    // Initialize the ROM with predefined values
    initial
    begin
        $readmemb("FPGA/Data/Simulation/rom.list", rom);
        q_instr = 32'b0;
        q_data = 32'b0;
    end

    // Port A read
    always @(posedge clk)
    begin
        q_instr <= rom[addr_instr];
    end

    // Port B read
    always @(posedge clk)
    begin
        q_data <= rom[addr_data];
    end

endmodule