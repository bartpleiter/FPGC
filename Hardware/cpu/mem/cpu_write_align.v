// Write alignment
// Puts the bytes that a write writes at their place in the word, and sets their byte
// enables. The other bytes are 0. An unaligned address is aligned down to the size of
// the write.
module cpu_write_align (
    input  wire [31:0] data,         // the data to write
    input  wire [1:0]  addr,         // bits [1:0] of the address
    input  wire        mem_byte,     // a word if neither byte nor half
    input  wire        mem_half,
    output reg  [31:0] write_data,
    output reg  [3:0]  byte_enable   // bit n enables bits [8n+7:8n]
);

always @(*) begin
    if (mem_byte) begin
        write_data  = {24'b0, data[7:0]} << {addr, 3'b000};
        byte_enable = 4'b0001 << addr;
    end else if (mem_half) begin
        write_data  = addr[1] ? {data[15:0], 16'b0} : {16'b0, data[15:0]};
        byte_enable = addr[1] ? 4'b1100 : 4'b0011;
    end else begin
        write_data  = data;
        byte_enable = 4'b1111;
    end
end

endmodule
