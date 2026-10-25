// Read alignment
// Takes the bytes that a read wants out of the word that holds them, and extends them to
// 32 bits. An unaligned address is aligned down to the size of the read.
module cpu_read_align (
    input  wire [31:0] word,         // the word from memory
    input  wire [1:0]  addr,         // bits [1:0] of the address
    input  wire        mem_byte,     // a word if neither byte nor half
    input  wire        mem_half,
    input  wire        sign_extend,  // readb, readh
    output reg  [31:0] value
);

wire [7:0]  byte_value = addr == 2'd0 ? word[7:0]   :
                         addr == 2'd1 ? word[15:8]  :
                         addr == 2'd2 ? word[23:16] :
                                        word[31:24];
wire [15:0] half_value = addr[1] ? word[31:16] : word[15:0];

always @(*) begin
    if (mem_byte)
        value = {{24{sign_extend && byte_value[7]}}, byte_value};
    else if (mem_half)
        value = {{16{sign_extend && half_value[15]}}, half_value};
    else
        value = word;
end

endmodule
