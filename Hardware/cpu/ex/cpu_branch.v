// Branch compare
// Compares a with b, signed or unsigned, and decides whether the branch condition holds.
module cpu_branch (
    input  wire [31:0] a,
    input  wire [31:0] b,
    input  wire [2:0]  branch_op,      // the condition
    input  wire        branch_signed,  // compare as signed numbers; beq and bne ignore it
    output reg         taken
);

wire eq = a == b;
wire lt = branch_signed ? $signed(a) < $signed(b) : a < b;

always @(*) begin
    case (branch_op)
        3'b000:  taken = eq;          // beq
        3'b001:  taken = !lt && !eq;  // bgt
        3'b010:  taken = !lt;         // bge
        3'b100:  taken = !eq;         // bne
        3'b101:  taken = lt;          // blt
        3'b110:  taken = lt || eq;    // ble
        default: taken = 1'b0;        // reserved
    endcase
end

endmodule
