/*
 * PixelEngine
 * Renders Pixel Plane at 320x240 on a 640x480 screen
 * Uses internal BRAM for pixel framebuffer (dual-port, no backpressure)
 */
module PixelEngine (
    // Video timings
    input wire          blank,
    input wire  [11:0]  h_count,   // Line position in pixels including blanking
    input wire  [11:0]  v_count,   // Frame position in lines including blanking

    // Output colors
    output wire [7:0]   r,
    output wire [7:0]   g,
    output wire [7:0]   b,

    // VRAM
    output wire [16:0]  vram_addr,
    input wire  [7:0]   vram_q
);

  localparam HSTART_HDMI = 159; // Pixel to start rendering
  localparam VSTART_HDMI = 44;  // Line to start rendering

  wire [9:0] h_start = HSTART_HDMI;
  wire [9:0] v_start = VSTART_HDMI;

  wire h_active = (h_count > h_start);
  wire v_active = (v_count > v_start);

  wire [9:0] line_active = (v_active) ? v_count - (v_start + 1'b1) : 10'd0;
  wire [9:0] pixel_active = (h_active && v_active) ? h_count - h_start : 10'd0;

  wire [16:0] pixel_idx = ((line_active >> 1) * 320) + (pixel_active >> 1);

  assign vram_addr = pixel_idx;

  // Output raw 8-bit pixel value as R3G3B2 expanded to 8-bit per channel
  // using bit replication (same as PixelPalette default mapping)
  assign r = (blank) ? 8'd0 : {vram_q[7:5], vram_q[7:5], vram_q[7:6]};
  assign g = (blank) ? 8'd0 : {vram_q[4:2], vram_q[4:2], vram_q[4:3]};
  assign b = (blank) ? 8'd0 : {vram_q[1:0], vram_q[1:0], vram_q[1:0], vram_q[1:0]};

endmodule
