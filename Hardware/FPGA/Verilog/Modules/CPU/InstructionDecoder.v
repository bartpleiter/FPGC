/*
 * InstructionDecoder
 * Very basic instruction decoder, mostly just extracts the fields from the instruction
 */
module InstructionDecoder (
    input  wire [31:0]  instr,

    output wire [3:0]   instr_op,
    output wire [3:0]   alu_op,
    output wire [2:0]   branch_op,

    output wire [31:0]  const_alu,
    output wire [31:0]  const_aluu,
    output wire [31:0]  const16,
    output wire [15:0]  const16u,
    output wire [26:0]  const27,

    output wire [3:0]   areg,
    output wire [3:0]   breg,
    output wire [3:0]   dreg,

    output wire         he,
    output wire         oe,
    output wire         sig,

    // Sub-opcode fields for byte-addressable memory operations
    output wire [3:0]   read_subop,   // bits [7:4] of READ instruction (unused BREG field)
    output wire [3:0]   write_subop   // bits [3:0] of WRITE instruction (unused DREG field)
);

assign instr_op  = instr[31:28];
assign alu_op    = instr[27:24];
assign branch_op = instr[3:1];

assign const_alu  = {{16{instr[23]}}, instr[23:8]}; // Sign extend to 32 bit
assign const_aluu = {16'd0,           instr[23:8]};
assign const16   = {{16{instr[27]}}, instr[27:12]}; // Sign extend to 32 bit
assign const16u  = instr[27:12];
assign const27   = instr[27:1];

// AREG is at a different position during an ARITH(M)C instruction
// We set BREG to 0 in that case
assign areg     = (instr_op == 4'b0001 || instr_op == 4'b0011) ? instr[7:4] : instr[11:8];
assign breg     = (instr_op == 4'b0001 || instr_op == 4'b0011) ? 4'd0 : instr[7:4];
assign dreg     = instr[3:0];

assign he       = instr[8];     // High-enable (loadhi)
assign oe       = instr[0];     // Offset-enable (jump[r])
assign sig      = instr[0];     // Signed comparison (branch)

// Sub-opcode fields for byte-addressable memory operations
// READ:  bits [7:4] encode the read size (0000=word, 0001=byte, 0010=half, 0101=byte-unsigned, 0110=half-unsigned)
// WRITE: bits [3:0] encode the write size (0000=word, 0001=byte, 0010=half)
// For non-READ/WRITE instructions these are don't-care
assign read_subop  = instr[7:4];  // Same position as BREG (unused in READ)
assign write_subop = instr[3:0];  // Same position as DREG (unused in WRITE)

endmodule
