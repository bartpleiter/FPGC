/*
 * ParallelComparator
 * High-speed 32-bit comparator using parallel tree structure
 * 
 * Replaces sequential carry-chain comparison with logarithmic-depth logic
 * to reduce critical path delay.
 * 
 * The comparator splits the 32-bit operands into 8 nibbles (4 bits each)
 * and computes local comparisons in parallel, then combines them.
 */
module ParallelComparator (
    input wire [31:0] a,
    input wire [31:0] b,
    input wire        sig,      // 1 = signed comparison, 0 = unsigned
    
    output wire       eq,       // a == b
    output wire       lt,       // a < b
    output wire       gt        // a > b
);

// For signed comparison, we need to handle the sign bit specially
wire a_sign = a[31];
wire b_sign = b[31];
wire sign_diff = a_sign ^ b_sign;

// ============================================================================
// STAGE 1: Nibble (4-bit) comparisons - 8 parallel comparisons
// ============================================================================
// For each nibble, compute: equal, a_greater, b_greater
wire [7:0] nibble_eq;      // nibble equality
wire [7:0] nibble_a_gt;    // a nibble > b nibble (unsigned)

// Nibble 7 (bits 31:28) - MSB
wire [3:0] a7 = a[31:28];
wire [3:0] b7 = b[31:28];
assign nibble_eq[7] = (a7 == b7);
assign nibble_a_gt[7] = (a7 > b7);

// Nibble 6 (bits 27:24)
wire [3:0] a6 = a[27:24];
wire [3:0] b6 = b[27:24];
assign nibble_eq[6] = (a6 == b6);
assign nibble_a_gt[6] = (a6 > b6);

// Nibble 5 (bits 23:20)
wire [3:0] a5 = a[23:20];
wire [3:0] b5 = b[23:20];
assign nibble_eq[5] = (a5 == b5);
assign nibble_a_gt[5] = (a5 > b5);

// Nibble 4 (bits 19:16)
wire [3:0] a4 = a[19:16];
wire [3:0] b4 = b[19:16];
assign nibble_eq[4] = (a4 == b4);
assign nibble_a_gt[4] = (a4 > b4);

// Nibble 3 (bits 15:12)
wire [3:0] a3 = a[15:12];
wire [3:0] b3 = b[15:12];
assign nibble_eq[3] = (a3 == b3);
assign nibble_a_gt[3] = (a3 > b3);

// Nibble 2 (bits 11:8)
wire [3:0] a2 = a[11:8];
wire [3:0] b2 = b[11:8];
assign nibble_eq[2] = (a2 == b2);
assign nibble_a_gt[2] = (a2 > b2);

// Nibble 1 (bits 7:4)
wire [3:0] a1 = a[7:4];
wire [3:0] b1 = b[7:4];
assign nibble_eq[1] = (a1 == b1);
assign nibble_a_gt[1] = (a1 > b1);

// Nibble 0 (bits 3:0) - LSB
wire [3:0] a0 = a[3:0];
wire [3:0] b0 = b[3:0];
assign nibble_eq[0] = (a0 == b0);
assign nibble_a_gt[0] = (a0 > b0);

// ============================================================================
// STAGE 2: Byte (8-bit) comparisons - 4 parallel comparisons
// Combine pairs of nibbles
// ============================================================================
wire [3:0] byte_eq;
wire [3:0] byte_a_gt;

// Byte 3 (nibbles 7,6) - bits 31:24
assign byte_eq[3] = nibble_eq[7] & nibble_eq[6];
assign byte_a_gt[3] = nibble_a_gt[7] | (nibble_eq[7] & nibble_a_gt[6]);

// Byte 2 (nibbles 5,4) - bits 23:16
assign byte_eq[2] = nibble_eq[5] & nibble_eq[4];
assign byte_a_gt[2] = nibble_a_gt[5] | (nibble_eq[5] & nibble_a_gt[4]);

// Byte 1 (nibbles 3,2) - bits 15:8
assign byte_eq[1] = nibble_eq[3] & nibble_eq[2];
assign byte_a_gt[1] = nibble_a_gt[3] | (nibble_eq[3] & nibble_a_gt[2]);

// Byte 0 (nibbles 1,0) - bits 7:0
assign byte_eq[0] = nibble_eq[1] & nibble_eq[0];
assign byte_a_gt[0] = nibble_a_gt[1] | (nibble_eq[1] & nibble_a_gt[0]);

// ============================================================================
// STAGE 3: Half-word (16-bit) comparisons - 2 parallel comparisons
// Combine pairs of bytes
// ============================================================================
wire [1:0] half_eq;
wire [1:0] half_a_gt;

// Half 1 (bytes 3,2) - bits 31:16
assign half_eq[1] = byte_eq[3] & byte_eq[2];
assign half_a_gt[1] = byte_a_gt[3] | (byte_eq[3] & byte_a_gt[2]);

// Half 0 (bytes 1,0) - bits 15:0
assign half_eq[0] = byte_eq[1] & byte_eq[0];
assign half_a_gt[0] = byte_a_gt[1] | (byte_eq[1] & byte_a_gt[0]);

// ============================================================================
// STAGE 4: Full 32-bit comparison
// Combine the two halves
// ============================================================================
wire full_eq = half_eq[1] & half_eq[0];
wire full_a_gt_unsigned = half_a_gt[1] | (half_eq[1] & half_a_gt[0]);

// ============================================================================
// Handle signed comparison
// For signed: if signs differ, negative number is smaller
// ============================================================================
wire a_gt_signed = sign_diff ? b_sign :  // Signs differ: a > b if b is negative
                   full_a_gt_unsigned;    // Signs same: use unsigned result

wire a_lt_signed = sign_diff ? a_sign :  // Signs differ: a < b if a is negative
                   (~full_eq & ~full_a_gt_unsigned);  // Signs same: a < b if not equal and not greater

// ============================================================================
// Output selection based on sig flag
// ============================================================================
assign eq = full_eq;
assign gt = sig ? a_gt_signed : full_a_gt_unsigned;
assign lt = sig ? a_lt_signed : (~full_eq & ~full_a_gt_unsigned);

endmodule
