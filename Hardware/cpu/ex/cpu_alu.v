// ALU
// Arithmetic Logic Unit for single-cycle operations.
// Performs combinational logic on a with b, producing the result y.
module cpu_alu (
    input  wire [3:0]  op,
    input  wire [31:0] a,
    input  wire [31:0] b,
    output reg  [31:0] y
);

always @(*) begin
    case (op)
        4'b0000: y = a | b;                             // or
        4'b0001: y = a & b;                             // and
        4'b0010: y = a ^ b;                             // xor
        4'b0011: y = a + b;                             // add
        4'b0100: y = a - b;                             // sub
        4'b0101: y = a << b[4:0];                       // shiftl
        4'b0110: y = a >> b[4:0];                       // shiftr
        4'b0111: y = ~a;                                // not
        4'b1010: y = {31'b0, $signed(a) < $signed(b)};  // slt
        4'b1011: y = {31'b0, a < b};                    // sltu
        4'b1100: y = b;                                 // load
        4'b1101: y = {b[15:0], a[15:0]};                // loadhi
        4'b1110: y = $signed(a) >>> b[4:0];             // shiftrs
        default: y = 32'bx;                             // reserved/unused
    endcase
end

endmodule
