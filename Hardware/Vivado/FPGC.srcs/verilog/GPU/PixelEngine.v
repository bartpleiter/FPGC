/*
 * Renders Pixel Plane at 320x240 on a 640x480 screen
 */
module PixelEngine (
    // Video timings
    input wire         blank,
    input wire [11:0]  h_count,   // Line position in pixels including blanking
    input wire [11:0]  v_count,   // Frame position in lines including blanking

    // Output colors
    output wire [2:0]  r,
    output wire [2:0]  g,
    output wire [1:0]  b,
    
    // VRAM
    output wire [16:0] vram_addr,
    input wire [7:0]   vram_q,

    // Parameters
    input wire         halfRes    // Render half res (160x120) at full res by zooming in at top left corner
);

  localparam HSTART_HDMI = 159; // Pixel to start rendering
  localparam VSTART_HDMI = 44;  // Line to start rendering

  wire [9:0] h_start = HSTART_HDMI;
  wire [9:0] v_start = VSTART_HDMI;

  wire h_active = (h_count > h_start);
  wire v_active = (v_count > v_start);

  wire [9:0] line_active = (v_active) ? v_count - (v_start + 1'b1) : 10'd0;
  wire [9:0] pixel_active = (h_active && v_active) ? h_count - h_start : 10'd0;

  wire [16:0] pixel_idx = (halfRes) ? (((line_active >> 1) >> 1) *320) + (pixel_active >> 2):
                                      ((line_active >> 1) *320) + (pixel_active >> 1);

  assign vram_addr = pixel_idx;

  assign r = (blank) ? 3'd0 : vram_q[7:5];
  assign g = (blank) ? 3'd0 : vram_q[4:2];
  assign b = (blank) ? 2'd0 : vram_q[1:0];

endmodule
