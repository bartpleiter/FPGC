/*
* Renders Background and Window plane
*/
module BGWrenderer(
    // Video I/O
    input               clk,
    input               hs,
    input               vs,
    input               blank,

    input               scale2x,    // render vertically in 2x scaling (e.g. for HDMI to get full screen 320x240 on a 640x480 signal)
                                    // note: horizontally this scaling is always applied

    // Output pixels
    output wire [2:0]   r,
    output wire [2:0]   g,
    output wire [1:0]   b,

    input [11:0]         h_count,  // line position in pixels including blanking 
    input [11:0]         v_count,  // frame position in lines including blanking 

    // VRAM32
    output [13:0]       vram32_addr,
    input  [31:0]       vram32_q, 

    // VRAM8
    output [13:0]       vram8_addr,
    input  [7:0]        vram8_q
);

localparam VSTART = 86; // Line to start rendering
localparam HSTART = 128; // About two tiles before blanking ends

// Actions based on hTilePixelCounter value
localparam
    fetch_bg_tile       = 0,
    fetch_pattern_bg    = 1,
    fetch_bg_color      = 2,
    fetch_palette_bg    = 3,

    fetch_wind_tile     = 4,
    fetch_pattern_wind  = 5,
    fetch_wind_color    = 6,
    fetch_palette_wind  = 7;

// Rendering offsets
reg [5:0] XtileOffset = 6'd0;
reg [2:0] XfineOffset = 3'd0;

// Tile and pixel counters
reg [5:0] hTileCounter = 6'd0; // 40 hTiles
reg [3:0] hTileDoublePixelCounter = 4'd0; // 16 pixels per tile
wire [2:0] hTilePixelCounter = hTileDoublePixelCounter[3:1]; // 8 pixels per tile

reg [4:0] vTileCounter = 5'd0; // 25 vTiles
reg [3:0] vTileDoubleLineCounter = 4'd0; // 16 pixels per tile
wire [2:0] vTileLineCounter = (scale2x) ?vTileDoubleLineCounter[3:1] : vTileDoubleLineCounter[2:0]; // 8 pixels per tile

reg [10:0] bg_tile = 11'd0;
reg [10:0] window_tile = 11'd0;
reg [10:0] bg_tile_line = 11'd0; // does not include the horizontal position
reg [10:0] window_tile_line = 11'd0; //does not include the horizontal position

wire [10:0] bg_tile_next = (h_count < HSTART && vTileCounter == 5'd0) ? XtileOffset : bg_tile + XtileOffset;
wire [10:0] window_tile_next = (h_count < HSTART && vTileCounter == 5'd0) ? 11'd0 : window_tile;


// Updating tile and pixel counters
always @(posedge clk)
begin
    if (!vs)
    begin
        bg_tile <= 11'd0;
        bg_tile_line <= 11'd0;
        // Window tile starts at -1, since it does not have a buffer for fine scrolling
        window_tile <= 11'b11111111111;
        window_tile_line <= 11'b11111111111;
    end

    // Horizontal counters
    if (h_count < HSTART || v_count < VSTART-1 || v_count >= VSTART-1 + 400)
    begin
        hTileCounter <= 6'd0;
        hTileDoublePixelCounter <= 4'd0;
        bg_tile <= bg_tile_line;
        window_tile <= window_tile_line;
    end
    else
    begin
        hTileDoublePixelCounter <= hTileDoublePixelCounter + 1'b1;
        if (hTileDoublePixelCounter == 4'd15)
        begin
            hTileCounter <= hTileCounter + 1'b1;
            bg_tile <= bg_tile + 1'b1;
            window_tile <= window_tile + 1'b1;
        end
    end

    // Vertical counters
    if (h_count == 12'd0)
    begin
        if (v_count < VSTART || v_count >= VSTART + 400)
        begin
            vTileCounter <= 5'd0;
            vTileDoubleLineCounter <= 4'd0;
            bg_tile <= 11'd0;
            window_tile <= 11'd0;
        end
        else 
        begin
            vTileDoubleLineCounter <= vTileDoubleLineCounter + 1'b1;
            if (scale2x)
            begin
                if (vTileDoubleLineCounter == 4'd15)
                begin
                    vTileCounter <= vTileCounter + 1'b1;
                    bg_tile_line <= bg_tile_line + 7'd64; // + number of tiles per line in bg plane
                    window_tile_line <= window_tile_line + 6'd40; // + number of tiles per line in window plane
                end
            end
            else
            begin
                if (vTileDoubleLineCounter == 4'd7)
                begin
                    vTileDoubleLineCounter <= 4'd0;
                    vTileCounter <= vTileCounter + 1'b1;
                    bg_tile_line <= bg_tile_line + 7'd64; // + number of tiles per line in bg plane
                    window_tile_line <= window_tile_line + 6'd40; // + number of tiles per line in window plane
                end
            end
        end
    end
end

// Data for next tile
reg [7:0]  tile_index           = 8'd0;
reg [7:0]  color_index          = 8'd0;
reg [15:0] pattern_bg_tile      = 16'd0;
reg [15:0] pattern_window_tile  = 16'd0;
reg [31:0] palette_bg_tile      = 32'd0;
//reg [31:0] palette_window_tile  = 32'd0; // should not be needed

// Data for current tile, should be updated at start of each tile
reg [15:0] current_pattern_bg_tile      = 16'd0;
reg [15:0] current_pattern_window_tile  = 16'd0;
reg [31:0] current_palette_bg_tile      = 32'd0;
reg [31:0] current_palette_window_tile  = 32'd0;

// Reading from VRAM
always @(posedge clk)
begin

    // Parameters
    if (h_count == 12'd1)
        XtileOffset <= vram8_q;
    if (h_count == 12'd2)
        XfineOffset <= vram8_q;

    if (hTileDoublePixelCounter[0])
    begin
        case (hTilePixelCounter)
            fetch_bg_tile:      tile_index <= vram8_q;

            fetch_pattern_bg:   begin
                                    if (vTileLineCounter[0])
                                        pattern_bg_tile <= vram32_q[15:0];
                                    else
                                        pattern_bg_tile <= vram32_q[31:16];
                                end

            fetch_bg_color:     color_index <= vram8_q;

            fetch_palette_bg:   palette_bg_tile <= vram32_q;

            fetch_wind_tile:    tile_index <= vram8_q;

            fetch_pattern_wind: begin
                                    if (vTileLineCounter[0])
                                        pattern_window_tile <= vram32_q[15:0];
                                    else
                                        pattern_window_tile <= vram32_q[31:16];
                                end

            fetch_wind_color:   color_index <= vram8_q;

            //fetch_palette_wind: palette_window_tile <= vram32_q; // is not needed, because the data can be directly put into current_palette_window_tile
        endcase
    end

    if (hTileDoublePixelCounter == 4'd15)
    begin
        current_pattern_bg_tile     <= pattern_bg_tile;
        current_pattern_window_tile <= pattern_window_tile;
        current_palette_bg_tile     <= palette_bg_tile;
        current_palette_window_tile <= vram32_q;
    end
end


assign vram8_addr = (h_count == 12'd0) ? 8192: // tile scroll offset
                    (h_count == 12'd1) ? 8193: // fine scroll offset
                    (hTilePixelCounter == fetch_bg_tile)    ? bg_tile_next:
                    (hTilePixelCounter == fetch_bg_color)   ? 2048  + bg_tile_next:
                    (hTilePixelCounter == fetch_wind_tile)  ? 4096  + window_tile_next:
                    (hTilePixelCounter == fetch_wind_color) ? 6144  + window_tile_next:
                    14'd0;

assign vram32_addr = (hTilePixelCounter == fetch_pattern_bg) ? (tile_index << 2) + (vTileLineCounter >> 1): //*4 because 4 addresses per tile
                     (hTilePixelCounter == fetch_palette_bg) ? 1024 + color_index:
                     (hTilePixelCounter == fetch_pattern_wind) ? (tile_index << 2) + (vTileLineCounter >> 1): //*4 because 4 addresses per tile
                     (hTilePixelCounter == fetch_palette_wind) ? 1024 + color_index:
                    14'd0;



// Pattern data for current pixel
reg [1:0] current_pixel_pattern_bg;
reg [1:0] current_pixel_pattern_window;

always @(*)
begin
    case (hTilePixelCounter)
        3'd0:   begin
                    current_pixel_pattern_bg <= current_pattern_bg_tile[15:14];
                    current_pixel_pattern_window <= current_pattern_window_tile[15:14];
                end
        3'd1:   begin
                    current_pixel_pattern_bg <= current_pattern_bg_tile[13:12];
                    current_pixel_pattern_window <= current_pattern_window_tile[13:12];
                end
        3'd2:   begin
                    current_pixel_pattern_bg <= current_pattern_bg_tile[11:10];
                    current_pixel_pattern_window <= current_pattern_window_tile[11:10];
                end
        3'd3:   begin
                    current_pixel_pattern_bg <= current_pattern_bg_tile[9:8];
                    current_pixel_pattern_window <= current_pattern_window_tile[9:8];
                end
        3'd4:   begin
                    current_pixel_pattern_bg <= current_pattern_bg_tile[7:6];
                    current_pixel_pattern_window <= current_pattern_window_tile[7:6];
                end
        3'd5:   begin
                    current_pixel_pattern_bg <= current_pattern_bg_tile[5:4];
                    current_pixel_pattern_window <= current_pattern_window_tile[5:4];
                end
        3'd6:   begin
                    current_pixel_pattern_bg <= current_pattern_bg_tile[3:2];
                    current_pixel_pattern_window <= current_pattern_window_tile[3:2];
                end
        3'd7:   begin
                    current_pixel_pattern_bg <= current_pattern_bg_tile[1:0];
                    current_pixel_pattern_window <= current_pattern_window_tile[1:0];
                end
    endcase

end


// Apply palette to current pixel pattern
reg [2:0] bg_r;
reg [2:0] bg_g;
reg [1:0] bg_b;

reg [2:0] window_r;
reg [2:0] window_g;
reg [1:0] window_b;

always @(*)
begin
    case (current_pixel_pattern_bg)
        2'b00:  begin
                    bg_r <= current_palette_bg_tile[31:29];
                    bg_g <= current_palette_bg_tile[28:26];
                    bg_b <= current_palette_bg_tile[25:24];
                end

        2'b01:  begin
                    bg_r <= current_palette_bg_tile[23:21];
                    bg_g <= current_palette_bg_tile[20:18];
                    bg_b <= current_palette_bg_tile[17:16];
                end

        2'b10:  begin
                    bg_r <= current_palette_bg_tile[15:13];
                    bg_g <= current_palette_bg_tile[12:10];
                    bg_b <= current_palette_bg_tile[9:8];
                end

        2'b11:  begin
                    bg_r <= current_palette_bg_tile[7:5];
                    bg_g <= current_palette_bg_tile[4:2];
                    bg_b <= current_palette_bg_tile[1:0];
                end
    endcase

    case (current_pixel_pattern_window)
        2'b00:  begin
                    window_r <= current_palette_window_tile[31:29];
                    window_g <= current_palette_window_tile[28:26];
                    window_b <= current_palette_window_tile[25:24];
                end

        2'b01:  begin
                    window_r <= current_palette_window_tile[23:21];
                    window_g <= current_palette_window_tile[20:18];
                    window_b <= current_palette_window_tile[17:16];
                end

        2'b10:  begin
                    window_r <= current_palette_window_tile[15:13];
                    window_g <= current_palette_window_tile[12:10];
                    window_b <= current_palette_window_tile[9:8];
                end

        2'b11:  begin
                    window_r <= current_palette_window_tile[7:5];
                    window_g <= current_palette_window_tile[4:2];
                    window_b <= current_palette_window_tile[1:0];
                end
    endcase
end

// Background pixel buffers for 8 pixels, so fine scrolling can be applied
reg [23:0] buf_bg_r = 24'd0;
reg [23:0] buf_bg_g = 24'd0;
reg [15:0] buf_bg_b = 16'd0;


always @(posedge clk)
begin
    // Do only once per two cycles, since we are using a double horizontal resolution
    if (h_count[0])
    begin
        buf_bg_r <= {buf_bg_r[20:0], bg_r};
        buf_bg_g <= {buf_bg_g[20:0], bg_g};
        buf_bg_b <= {buf_bg_b[13:0], bg_b};
    end
end

reg [2:0] bg_r_sel;
reg [2:0] bg_g_sel;
reg [1:0] bg_b_sel;

always @(*)
begin
    case (XfineOffset)
        3'd0:   begin
                    bg_r_sel <= buf_bg_r[23:21];
                    bg_g_sel <= buf_bg_g[23:21];
                    bg_b_sel <= buf_bg_b[15:14];
                end
        3'd1:   begin
                    bg_r_sel <= buf_bg_r[20:18];
                    bg_g_sel <= buf_bg_g[20:18];
                    bg_b_sel <= buf_bg_b[13:12];
                end
        3'd2:   begin
                    bg_r_sel <= buf_bg_r[17:15];
                    bg_g_sel <= buf_bg_g[17:15];
                    bg_b_sel <= buf_bg_b[11:10];
                end
        3'd3:   begin
                    bg_r_sel <= buf_bg_r[14:12];
                    bg_g_sel <= buf_bg_g[14:12];
                    bg_b_sel <= buf_bg_b[9:8];
                end
        3'd4:   begin
                    bg_r_sel <= buf_bg_r[11:9];
                    bg_g_sel <= buf_bg_g[11:9];
                    bg_b_sel <= buf_bg_b[7:6];
                end
        3'd5:   begin
                    bg_r_sel <= buf_bg_r[8:6];
                    bg_g_sel <= buf_bg_g[8:6];
                    bg_b_sel <= buf_bg_b[5:4];
                end
        3'd6:   begin
                    bg_r_sel <= buf_bg_r[5:3];
                    bg_g_sel <= buf_bg_g[5:3];
                    bg_b_sel <= buf_bg_b[3:2];
                end
        3'd7:   begin
                    bg_r_sel <= buf_bg_r[2:0];
                    bg_g_sel <= buf_bg_g[2:0];
                    bg_b_sel <= buf_bg_b[1:0];
                end
    endcase
end

// Give priority to the window layer. Current method is by checking for a black palette for 0b00 pixels

wire bgPriority = current_pixel_pattern_window == 2'b00 && current_palette_window_tile[31:24] == 8'd0;
assign r = (bgPriority) ? bg_r_sel : window_r;
assign g = (bgPriority) ? bg_g_sel : window_g;
assign b = (bgPriority) ? bg_b_sel : window_b;

endmodule