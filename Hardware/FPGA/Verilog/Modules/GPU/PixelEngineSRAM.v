/*
 * PixelEngineSRAM
 * 
 * Renders Pixel Plane at 320×240 on a 640×480 screen (2× scaling)
 * 
 * Timing:
 * - 640×480 @ 25MHz = 800×525 total
 * - Active area starts at h_count=160, v_count=45
 * - Each 320×240 source pixel is displayed as 2×2 output pixels
 */
module PixelEngineSRAM (
    // Video timings
    input wire          clkPixel,  // 25MHz GPU clock
    input wire          blank,
    input wire  [11:0]  h_count,   // Line position in pixels including blanking
    input wire  [11:0]  v_count,   // Frame position in lines including blanking

    // Output colors
    output wire [2:0]   r,
    output wire [2:0]   g,
    output wire [1:0]   b,

    // SRAM interface
    output wire [16:0]  sram_addr,     // Requested pixel address
    input wire  [7:0]   sram_data,     // Pixel data from SRAM
    
    // Line buffer status (high when using line buffer, SRAM not needed)
    output wire         using_line_buffer,

    // Parameters
    input wire          halfRes    // Render half res (160×120) at full res
);

    // halfRes is disabled for now

    // VGA timing constants
    localparam HSTART_HDMI = 159; // Pixel to start rendering
    localparam VSTART_HDMI = 44;  // Line to start rendering

    wire [9:0] h_start = HSTART_HDMI;
    wire [9:0] v_start = VSTART_HDMI;

    // Active region detection
    wire h_active = (h_count > h_start);
    wire v_active = (v_count > v_start);

    // Position within active area (0-based)
    wire [9:0] line_active = (v_active) ? v_count - (v_start + 1'b1) : 10'd0;
    wire [9:0] pixel_active = (h_active && v_active) ? h_count - (h_start + 1'b1) : 10'd0;

    // Determine if we're in active video region
    wire in_active_video = h_active && v_active && (line_active < 480) && (pixel_active < 640);

    // Source pixel coordinates (after 2× or 4× scaling)
    // Normal mode: 640×480 -> 320×240 (÷2)
    // Half res mode: 640×480 -> 160×120 (÷4)
    wire [8:0] source_x = pixel_active[9:1];
    wire [7:0] source_y = line_active[9:1];
    
    // Calculate pixel address: y * 320 + x
    // For halfRes: y * 320 + x (where x is 0-159, y is 0-119)
    // We need to calculate: source_y * 320 + source_x
    // 320 = 256 + 64 = (y << 8) + (y << 6)
    wire [16:0] pixel_addr = ({9'd0, source_y} << 8) + ({9'd0, source_y} << 6) + {8'd0, source_x};
    
    // Output pixel address for SRAM read
    assign sram_addr = pixel_addr;

    // Line buffer for vertical 2× scaling
    // Stores one source row (320 pixels for normal mode, 160 for halfRes)
    // When on even lines (0, 2, 4...), we read from SRAM and fill line buffer
    // When on odd lines (1, 3, 5...), we read from line buffer (same source row)
    reg [7:0] line_buffer [0:319];
    integer lb_i;
    initial begin
        for (lb_i = 0; lb_i < 320; lb_i = lb_i + 1) begin
            line_buffer[lb_i] = 8'd0;
        end
    end

    // Is this the first display line of a source row pair?
    // For 2× vertical: even lines (0, 2, 4, ...) are first of pair
    // For 4× vertical (halfRes): lines 0, 4, 8, ... are first of quad
    wire first_line_of_pair = (line_active[0] == 1'b0);
    
    // Signal when line buffer is being used (odd lines don't need SRAM reads)
    // Also safe during blanking since we're not displaying anything
    assign using_line_buffer = !first_line_of_pair && in_active_video;
    
    // Is this the first display pixel of a source pixel pair?
    // For 2× horizontal: even pixels (0, 2, 4, ...) are first of pair
    // For 4× horizontal (halfRes): pixels 0, 4, 8, ... are first of quad
    wire first_pixel_of_pair = (pixel_active[0] == 1'b0);

    // Pixel data source selection:
    // - First line of pair: from SRAM (and store in line buffer)
    // - Second line of pair: from line buffer
    wire [7:0] line_buffer_data = line_buffer[source_x];
    wire [7:0] pixel_source = first_line_of_pair ? sram_data : line_buffer_data;
    
    // Pixel holding register for horizontal 2× scaling
    reg [7:0] pixel_hold = 8'd0;

    always @(posedge clkPixel) begin
        // On first line of pair, store SRAM data to line buffer
        // Only store on first pixel of horizontal pair (avoid duplicate writes)
        if (in_active_video && first_line_of_pair && first_pixel_of_pair) begin
            line_buffer[source_x] <= sram_data;
        end
        
        // Update pixel hold on first pixel of horizontal pair
        if (in_active_video && first_pixel_of_pair) begin
            pixel_hold <= pixel_source;
        end
    end
    
    // Display pixel: 
    // - First pixel of pair: use pixel_source (combinatorial from SRAM or line buffer)
    // - Second pixel of pair: use pixel_hold (registered)
    wire [7:0] display_pixel = first_pixel_of_pair ? pixel_source : pixel_hold;
    
    // Output RGB values
    assign r = (blank) ? 3'd0 : display_pixel[7:5];
    assign g = (blank) ? 3'd0 : display_pixel[4:2];
    assign b = (blank) ? 2'd0 : display_pixel[1:0];

endmodule
