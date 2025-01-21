/*
 * Converts an 8-bit RGB value (RRRGGGBB) to a 24-bit RGB value (RRRRRRRR GGGGGGGG BBBBBBBB).
 */
module RGB8toRGB24 (
    input wire [ 7:0]  rgb8, // 8-bit RGB input (RRRGGGBB)
    output wire [23:0] rgb24 // 24-bit RGB output (RRRRRRRR GGGGGGGG BBBBBBBB)
);

wire [2:0] r_in = rgb8[7:5];  // Extract 3 bits for red
wire [2:0] g_in = rgb8[4:2];  // Extract 3 bits for green
wire [1:0] b_in = rgb8[1:0];  // Extract 2 bits for blue

// Expand each channel to 8 bits using replication
wire [7:0] r_out = {r_in, r_in, r_in[2:0]};
wire [7:0] g_out = {g_in, g_in, g_in[2:0]};
wire [7:0] b_out = {b_in, b_in, b_in, b_in};

// Combine the expanded channels into a 24-bit RGB value
assign rgb24 = {r_out, g_out, b_out};

endmodule
