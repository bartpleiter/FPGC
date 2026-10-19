// Register file
// 15 registers of 32 bits in flip-flops, with two read ports and one write port.
// r0 is always 0 and cannot be overwritten.
module cpu_regfile (
    input  wire        clk,
    input  wire        reset,

    // Read ports, used in ID
    input  wire [3:0]  areg,
    input  wire [3:0]  breg,
    output wire [31:0] areg_value,
    output wire [31:0] breg_value,

    // Write port, driven by WB
    input  wire        writes_dreg,
    input  wire [3:0]  dreg,
    input  wire [31:0] dreg_value
);

reg [31:0] regs [1:15];

// Write-through: a register that is written in this cycle reads as its new value.
assign areg_value = areg == 4'd0                ? 32'd0 :
                    writes_dreg && dreg == areg ? dreg_value :
                                                  regs[areg];
assign breg_value = breg == 4'd0                ? 32'd0 :
                    writes_dreg && dreg == breg ? dreg_value :
                                                  regs[breg];

integer i;
always @(posedge clk) begin
    if (reset) begin
        // Clear all registers to 0 on reset
        for (i = 1; i <= 15; i = i + 1)
            regs[i] <= 32'd0;
    end else if (writes_dreg && dreg != 4'd0) begin
        // Write dreg
        regs[dreg] <= dreg_value;
    end
end

endmodule
