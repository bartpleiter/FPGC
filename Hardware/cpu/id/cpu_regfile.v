// Register file
module cpu_regfile (
    input  wire        clk,

    // Read ports
    input  wire [3:0]  areg,
    input  wire [3:0]  breg,
    output wire [31:0] areg_value,
    output wire [31:0] breg_value,

    // Write port
    input  wire        writes_dreg,
    input  wire [3:0]  dreg,
    input  wire [31:0] dreg_value
);

reg [31:0] regs [1:15];

// A register that is written in this cycle reads as its new value (write-through).
assign areg_value = areg == 4'd0                ? 32'd0 :
                    writes_dreg && dreg == areg ? dreg_value :
                                                  regs[areg];
assign breg_value = breg == 4'd0                ? 32'd0 :
                    writes_dreg && dreg == breg ? dreg_value :
                                                  regs[breg];

// No initialization so Quartus can place the registers in block RAM
always @(posedge clk)
    if (writes_dreg && dreg != 4'd0)
        regs[dreg] <= dreg_value;

endmodule
