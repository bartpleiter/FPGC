/*
 * PixelEngineSRAM
 * Modified PixelEngine that reads from FIFO instead of VRAM
 * The FIFO is fed by the SRAM arbiter which prefetches pixels from external SRAM
 * 
 * Renders Pixel Plane at 320x240 on a 640x480 screen (2x scaling)
 * 
 * Key insight: With FIFO-based approach, we read each pixel exactly once.
 * For 2x vertical scaling, we use a 320-pixel line buffer to store one source row.
 * Even display lines read from FIFO and fill line buffer.
 * Odd display lines read from line buffer (same source row).
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

    // FIFO interface (replaces direct VRAM access)
    input wire  [7:0]   fifo_data,
    input wire          fifo_empty,
    output wire         fifo_rd_en,

    // Parameters
    input wire          halfRes    // Render half res (160x120) at full res
);

    localparam HSTART_HDMI = 159; // Pixel to start rendering
    localparam VSTART_HDMI = 44;  // Line to start rendering

    wire [9:0] h_start = HSTART_HDMI;
    wire [9:0] v_start = VSTART_HDMI;

    wire h_active = (h_count > h_start);
    wire v_active = (v_count > v_start);

    wire [9:0] line_active = (v_active) ? v_count - (v_start + 1'b1) : 10'd0;
    wire [9:0] pixel_active = (h_active && v_active) ? h_count - (h_start + 1'b1) : 10'd0;

    // Determine if we're in active video region
    wire in_active_video = h_active && v_active && (line_active < 480) && (pixel_active < 640);

    // Line buffer for vertical 2x scaling
    // Stores one source row (320 pixels for normal mode, 160 for halfRes)
    // Initialized to 0 for simulation
    reg [7:0] line_buffer [0:319];
    integer lb_i;
    initial begin
        for (lb_i = 0; lb_i < 320; lb_i = lb_i + 1) begin
            line_buffer[lb_i] = 8'd0;
        end
    end
    
    // Current source pixel X position (0-319 for normal, 0-159 for halfRes)
    wire [8:0] source_x = halfRes ? pixel_active[9:2] : pixel_active[9:1];
    
    // Current source row (0-239 for normal, 0-119 for halfRes)
    wire [7:0] source_y = halfRes ? line_active[9:2] : line_active[9:1];
    
    // Is this the first display line of a source row pair?
    // For 2x vertical: even lines (0, 2, 4, ...) are first of pair
    // For 4x vertical (halfRes): lines 0, 4, 8, ... are first of quad
    wire first_line_of_pair = halfRes ? (line_active[1:0] == 2'b00) : (line_active[0] == 1'b0);
    
    // Is this the first display pixel of a source pixel pair?
    // For 2x horizontal: even pixels (0, 2, 4, ...) are first of pair
    // For 4x horizontal (halfRes): pixels 0, 4, 8, ... are first of quad
    wire first_pixel_of_pair = halfRes ? (pixel_active[1:0] == 2'b00) : (pixel_active[0] == 1'b0);
    
    // Read from FIFO on:
    // 1. First line of each vertical pair (even display lines)
    // 2. First pixel of each horizontal pair (even display pixels)
    // 3. In active video region
    // 4. FIFO not empty
    wire do_fifo_read = in_active_video && first_line_of_pair && first_pixel_of_pair && !fifo_empty;
    
    assign fifo_rd_en = do_fifo_read;
    
    // Pixel holding register for horizontal scaling
    reg [7:0] pixel_hold = 8'd0;
    
    // Combinatorial line buffer read (available same cycle)
    wire [7:0] line_buffer_data = line_buffer[source_x];
    
    // Select pixel source:
    // - First line of pair with FIFO data: from FIFO
    // - First line of pair without FIFO data: use held value (underflow - should not happen)
    // - Second line of pair: from line buffer
    wire [7:0] pixel_source = first_line_of_pair ? 
                              (fifo_empty ? pixel_hold : fifo_data) : 
                              line_buffer_data;
    
    always @(posedge clkPixel) begin
        // On first line of pair, first pixel of h-pair: read from FIFO, store to line buffer
        if (do_fifo_read) begin
            line_buffer[source_x] <= fifo_data;
        end
        
        // Update pixel hold on first pixel of horizontal pair (when in active video)
        if (in_active_video && first_pixel_of_pair) begin
            pixel_hold <= pixel_source;
        end
    end
    
    // Display pixel: use pixel_source for first pixel of pair (combinatorial)
    // use pixel_hold for second pixel of pair (registered from previous cycle)
    wire [7:0] display_pixel = first_pixel_of_pair ? pixel_source : pixel_hold;
    
    // Output RGB values
    assign r = (blank) ? 3'd0 : display_pixel[7:5];
    assign g = (blank) ? 3'd0 : display_pixel[4:2];
    assign b = (blank) ? 2'd0 : display_pixel[1:0];

endmodule
