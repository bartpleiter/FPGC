// Graphics library for the kernel to allow terminal-like text output
// Uses the Window layer of the FSX2 GPU for text rendering
// Window layer: 40x25 tiles, each tile 8x8 pixels = 320x200 pixels

// Memory addresses for GPU VRAM
#define GFX_PATTERN_TABLE_ADDR 0x7900000  // 256 patterns × 4 words each
#define GFX_PALETTE_TABLE_ADDR 0x7900400  // 32 palettes × 1 word each (offset 1024)
#define GFX_BG_TILE_ADDR       0x7A00000  // Background tile table
#define GFX_BG_COLOR_ADDR      0x7A00800  // Background color table (offset 2048)
#define GFX_WINDOW_TILE_ADDR   0x7A01000  // Window tile table (offset 4096)
#define GFX_WINDOW_COLOR_ADDR  0x7A01800  // Window color table (offset 6144)

// Terminal dimensions
#define GFX_COLS 40
#define GFX_ROWS 25

// Terminal state (global)
int gfx_cursor_x = 0;
int gfx_cursor_y = 0;
int gfx_saved_cursor_x = 0;
int gfx_saved_cursor_y = 0;
int gfx_fg_color = 0;  // Palette index for text
int gfx_bg_color = 0;  // Not used directly, part of palette
int gfx_cursor_visible = 1;
int gfx_scroll_top = 0;
int gfx_scroll_bottom = 24;

// Function declarations
void GFX_init(void);
void GFX_copy_palette_table(void);
void GFX_copy_pattern_table(void);

void GFX_cursor_set(int x, int y);
void GFX_cursor_get(int *x, int *y);
void GFX_cursor_save(void);
void GFX_cursor_restore(void);

void GFX_putchar(char c);
void GFX_putchar_at(char c, int x, int y);
void GFX_puts(char *str);
void GFX_write(char *buf, int len);

void GFX_clear(void);
void GFX_clear_line(int y);
void GFX_clear_from_cursor(void);
void GFX_clear_line_from_cursor(void);
void GFX_scroll_up(int lines);
void GFX_scroll_down(int lines);

void GFX_set_color(int palette_idx);
void GFX_get_dimensions(int *width, int *height);
void GFX_set_scroll_region(int top, int bottom);

// Helper to write to memory-mapped address
void GFX_write_vram32(int addr, int value)
{
    int *ptr;
    ptr = (int *)addr;
    *ptr = value;
}

// Helper to write to VRAM8 (8-bit memory region, but word-addressed)
void GFX_write_vram8(int addr, int value)
{
    int *ptr;
    ptr = (int *)addr;
    *ptr = value;
}

// Helper to read from VRAM8
int GFX_read_vram8(int addr)
{
    int *ptr;
    ptr = (int *)addr;
    return *ptr;
}

// Initialize the graphics system
void GFX_init(void)
{
    // Reset cursor position
    gfx_cursor_x = 0;
    gfx_cursor_y = 0;
    gfx_fg_color = 0;
    gfx_scroll_top = 0;
    gfx_scroll_bottom = GFX_ROWS - 1;

    // Clear the window layer
    GFX_clear();
}

// Copy the default palette table from ascii_data.c to VRAM
// Palette format: each word contains 4 colors (8 bits each in R3G3B2 format)
// Palette 0 is used for text: color0=bg, color1=fg, color2/3=unused
void GFX_copy_palette_table(void)
{
    int i;
    int *src;
    int palette_addr;

    // Get address of DATA_PALETTE_DEFAULT function
    // The data follows the function prologue, need to calculate offset
    // DATA_PALETTE_DEFAULT is a function containing inline asm with .dw directives
    // The actual data starts after the function entry code
    // Based on the structure, data is right after the function label
    // We use addr2reg through inline asm to get the address

    // For simplicity, we directly write a default palette here
    // Palette 0: black background, white foreground
    // Format: [color3][color2][color1][color0] each 8 bits R3G3B2
    // White = 0xFF (all bits set), Black = 0x00

    palette_addr = GFX_PALETTE_TABLE_ADDR;

    // Palette 0: black bg (00), white fg (FF), unused, unused
    // Word format: color0 | (color1 << 8) | (color2 << 16) | (color3 << 24)
    // But GPU reads: [31:24]=color0, [23:16]=color1, [15:8]=color2, [7:0]=color3
    // So for: color0=black(0x00), color1=white(0xFF), color2=black, color3=black
    // Word = 0x00FF0000
    GFX_write_vram32(palette_addr, 0x00FF0000);

    // Fill remaining palettes with same default
    for (i = 1; i < 32; i++)
    {
        GFX_write_vram32(palette_addr + i, 0x00FF0000);
    }
}

// Copy the default ASCII pattern table to VRAM
// Each character is 8x8 pixels, 2 bits per pixel = 16 bits per line
// 8 lines per char, but stored as 2 lines per word = 4 words per character
void GFX_copy_pattern_table(void)
{
    int i;
    int *src;
    int *dst;
    int pattern_addr;
    int word_count;

    // The pattern data is stored in DATA_ASCII_DEFAULT as inline asm .dw
    // 256 characters × 4 words = 1024 words total
    // We need to copy this to VRAM32 pattern table area

    pattern_addr = GFX_PATTERN_TABLE_ADDR;
    word_count = 1024; // 256 patterns × 4 words each

    // Get the address of the data using inline assembly
    // The data follows immediately after DATA_ASCII_DEFAULT function entry
    asm(
        "addr2reg DATA_ASCII_DEFAULT_data r1"
    );

    // Copy using assembly since we need the address in r1
    asm(
        "load32 0x7900000 r2"   // destination: pattern table
        "load32 1024 r3"        // word count
        "GFX_copy_pattern_loop:"
        "  read 0 r1 r4"        // read from source
        "  write 0 r2 r4"       // write to dest
        "  add r1 1 r1"         // increment source
        "  add r2 1 r2"         // increment dest
        "  sub r3 1 r3"         // decrement counter
        "  bne r0 r3 2"         // if counter != 0, loop
        "    jump GFX_copy_pattern_loop"
    );
}

// Set cursor position
void GFX_cursor_set(int x, int y)
{
    if (x >= 0 && x < GFX_COLS)
        gfx_cursor_x = x;
    if (y >= 0 && y < GFX_ROWS)
        gfx_cursor_y = y;
}

// Get cursor position
void GFX_cursor_get(int *x, int *y)
{
    *x = gfx_cursor_x;
    *y = gfx_cursor_y;
}

// Save cursor position
void GFX_cursor_save(void)
{
    gfx_saved_cursor_x = gfx_cursor_x;
    gfx_saved_cursor_y = gfx_cursor_y;
}

// Restore saved cursor position
void GFX_cursor_restore(void)
{
    gfx_cursor_x = gfx_saved_cursor_x;
    gfx_cursor_y = gfx_saved_cursor_y;
}

// Print a character at specific position without moving cursor
void GFX_putchar_at(char c, int x, int y)
{
    int tile_addr;
    int color_addr;

    if (x < 0 || x >= GFX_COLS || y < 0 || y >= GFX_ROWS)
        return;

    // Calculate tile address: window tile table + (y * 40 + x)
    tile_addr = GFX_WINDOW_TILE_ADDR + (y * GFX_COLS) + x;

    // Calculate color address: window color table + (y * 40 + x)
    color_addr = GFX_WINDOW_COLOR_ADDR + (y * GFX_COLS) + x;

    // Write the ASCII value as tile index (pattern table has ASCII chars)
    GFX_write_vram8(tile_addr, (int)c);

    // Write the palette index
    GFX_write_vram8(color_addr, gfx_fg_color);
}

// Print a character at cursor and advance
void GFX_putchar(char c)
{
    // Handle special characters
    if (c == '\n')
    {
        gfx_cursor_x = 0;
        gfx_cursor_y++;
    }
    else if (c == '\r')
    {
        gfx_cursor_x = 0;
    }
    else if (c == '\t')
    {
        // Tab: move to next multiple of 4
        gfx_cursor_x = (gfx_cursor_x + 4) & ~3;
    }
    else if (c == '\b')
    {
        // Backspace
        if (gfx_cursor_x > 0)
            gfx_cursor_x--;
    }
    else
    {
        // Regular character
        GFX_putchar_at(c, gfx_cursor_x, gfx_cursor_y);
        gfx_cursor_x++;
    }

    // Handle line wrap
    if (gfx_cursor_x >= GFX_COLS)
    {
        gfx_cursor_x = 0;
        gfx_cursor_y++;
    }

    // Handle scroll when reaching bottom
    if (gfx_cursor_y > gfx_scroll_bottom)
    {
        GFX_scroll_up(1);
        gfx_cursor_y = gfx_scroll_bottom;
    }
}

// Print a null-terminated string
void GFX_puts(char *str)
{
    while (*str)
    {
        GFX_putchar(*str);
        str++;
    }
}

// Write a buffer of specific length
void GFX_write(char *buf, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        GFX_putchar(buf[i]);
    }
}

// Clear the entire screen
void GFX_clear(void)
{
    int i;
    int tile_addr;
    int color_addr;
    int total_tiles;

    tile_addr = GFX_WINDOW_TILE_ADDR;
    color_addr = GFX_WINDOW_COLOR_ADDR;
    total_tiles = GFX_COLS * GFX_ROWS;

    for (i = 0; i < total_tiles; i++)
    {
        GFX_write_vram8(tile_addr + i, 0);      // Space character (or 0)
        GFX_write_vram8(color_addr + i, gfx_fg_color);
    }

    gfx_cursor_x = 0;
    gfx_cursor_y = 0;
}

// Clear a specific line
void GFX_clear_line(int y)
{
    int i;
    int tile_addr;
    int color_addr;

    if (y < 0 || y >= GFX_ROWS)
        return;

    tile_addr = GFX_WINDOW_TILE_ADDR + (y * GFX_COLS);
    color_addr = GFX_WINDOW_COLOR_ADDR + (y * GFX_COLS);

    for (i = 0; i < GFX_COLS; i++)
    {
        GFX_write_vram8(tile_addr + i, 0);
        GFX_write_vram8(color_addr + i, gfx_fg_color);
    }
}

// Clear from cursor to end of screen
void GFX_clear_from_cursor(void)
{
    int i;
    int y;

    // Clear rest of current line
    GFX_clear_line_from_cursor();

    // Clear all lines below
    for (y = gfx_cursor_y + 1; y < GFX_ROWS; y++)
    {
        GFX_clear_line(y);
    }
}

// Clear from cursor to end of line
void GFX_clear_line_from_cursor(void)
{
    int i;
    int tile_addr;
    int color_addr;

    tile_addr = GFX_WINDOW_TILE_ADDR + (gfx_cursor_y * GFX_COLS) + gfx_cursor_x;
    color_addr = GFX_WINDOW_COLOR_ADDR + (gfx_cursor_y * GFX_COLS) + gfx_cursor_x;

    for (i = gfx_cursor_x; i < GFX_COLS; i++)
    {
        GFX_write_vram8(tile_addr + (i - gfx_cursor_x), 0);
        GFX_write_vram8(color_addr + (i - gfx_cursor_x), gfx_fg_color);
    }
}

// Scroll up by N lines
void GFX_scroll_up(int lines)
{
    int y, x;
    int src_tile, dst_tile;
    int src_color, dst_color;
    int tile_val, color_val;

    if (lines <= 0)
        return;

    // Move lines up
    for (y = gfx_scroll_top; y <= gfx_scroll_bottom - lines; y++)
    {
        src_tile = GFX_WINDOW_TILE_ADDR + ((y + lines) * GFX_COLS);
        dst_tile = GFX_WINDOW_TILE_ADDR + (y * GFX_COLS);
        src_color = GFX_WINDOW_COLOR_ADDR + ((y + lines) * GFX_COLS);
        dst_color = GFX_WINDOW_COLOR_ADDR + (y * GFX_COLS);

        for (x = 0; x < GFX_COLS; x++)
        {
            tile_val = GFX_read_vram8(src_tile + x);
            color_val = GFX_read_vram8(src_color + x);
            GFX_write_vram8(dst_tile + x, tile_val);
            GFX_write_vram8(dst_color + x, color_val);
        }
    }

    // Clear the bottom lines
    for (y = gfx_scroll_bottom - lines + 1; y <= gfx_scroll_bottom; y++)
    {
        GFX_clear_line(y);
    }
}

// Scroll down by N lines
void GFX_scroll_down(int lines)
{
    int y, x;
    int src_tile, dst_tile;
    int src_color, dst_color;
    int tile_val, color_val;

    if (lines <= 0)
        return;

    // Move lines down (start from bottom)
    for (y = gfx_scroll_bottom; y >= gfx_scroll_top + lines; y--)
    {
        src_tile = GFX_WINDOW_TILE_ADDR + ((y - lines) * GFX_COLS);
        dst_tile = GFX_WINDOW_TILE_ADDR + (y * GFX_COLS);
        src_color = GFX_WINDOW_COLOR_ADDR + ((y - lines) * GFX_COLS);
        dst_color = GFX_WINDOW_COLOR_ADDR + (y * GFX_COLS);

        for (x = 0; x < GFX_COLS; x++)
        {
            tile_val = GFX_read_vram8(src_tile + x);
            color_val = GFX_read_vram8(src_color + x);
            GFX_write_vram8(dst_tile + x, tile_val);
            GFX_write_vram8(dst_color + x, color_val);
        }
    }

    // Clear the top lines
    for (y = gfx_scroll_top; y < gfx_scroll_top + lines; y++)
    {
        GFX_clear_line(y);
    }
}

// Set the palette index for text
void GFX_set_color(int palette_idx)
{
    if (palette_idx >= 0 && palette_idx < 32)
        gfx_fg_color = palette_idx;
}

// Get terminal dimensions
void GFX_get_dimensions(int *width, int *height)
{
    *width = GFX_COLS;
    *height = GFX_ROWS;
}

// Set scrolling region
void GFX_set_scroll_region(int top, int bottom)
{
    if (top >= 0 && top < GFX_ROWS && bottom >= top && bottom < GFX_ROWS)
    {
        gfx_scroll_top = top;
        gfx_scroll_bottom = bottom;
    }
}
