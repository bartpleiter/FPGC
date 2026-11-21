// Boot ROM
// 4 KiB with two read ports. Its contents come from a hex file.
module rom #(
    parameter CONTENTS = "rom.hex"  // one hex word per line, for $readmemh
) (
    input  wire        clk,

    // Instruction fetch
    input  wire        enable_a,
    input  wire [11:2] addr_a,
    output reg  [31:0] word_a,

    // Data reads
    input  wire [11:2] addr_b,
    output reg  [31:0] word_b
);

reg [31:0] words [0:1023];

initial $readmemh(CONTENTS, words);

always @(posedge clk) begin
    if (enable_a)
        word_a <= words[addr_a];
    word_b <= words[addr_b];
end

endmodule
