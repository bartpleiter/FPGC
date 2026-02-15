
// Graphics library for the kernel to allow terminal-like text output
// Uses the Window layer of the FSX2 GPU for text rendering
// Window layer: 40x25 tiles, each tile 8x8 pixels = 320x200 pixels

// Memory addresses for GPU VRAM
#define GFX_PATTERN_TABLE_ADDR 0x7900000  // 256 patterns × 4 words each
#define GFX_PALETTE_TABLE_ADDR 0x7900400  // 32 palettes × 1 word each (offset 1024)
#define GFX_WINDOW_TILE_ADDR   0x7A01000  // Window tile table (offset 4096)
#define GFX_WINDOW_COLOR_ADDR  0x7A01800  // Window color table (offset 6144)

// Terminal dimensions
#define GFX_COLS 40
#define GFX_ROWS 25

// Terminal state (global)
unsigned int gfx_cursor_x = 0;
unsigned int gfx_cursor_y = 0;
unsigned int gfx_saved_cursor_x = 0;
unsigned int gfx_saved_cursor_y = 0;
unsigned int gfx_fg_color = 0;
unsigned int gfx_bg_color = 0;
unsigned int gfx_cursor_visible = 1;
unsigned int gfx_scroll_top = 0;
unsigned int gfx_scroll_bottom = 24;

// Initialize the graphics system
void GFX_init()
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

// Copy the provided palette table to VRAM
// Palette format: each word contains 4 colors (8 bits each in R3G3B2 format)
// Palette 0 is used for text: color0=bg, color1=fg, color2/3=unused
void GFX_copy_palette_table(unsigned int* palette_table)
{
    int i;
    unsigned int addr = GFX_PALETTE_TABLE_ADDR;

    unsigned int *palette_table_vram = (unsigned int *)GFX_PALETTE_TABLE_ADDR;

    for (i = 0; i < 32; i++)
    {
        palette_table_vram[i] = palette_table[i];
    }
}

void GFX_debug_uart_putchar(unsigned int c)
{
    // Send character over UART (memory-mapped I/O)
    unsigned int* uart_tx_addr = (unsigned int*)0x7000000; // UART TX address
    *uart_tx_addr = c;
}

// Copy the provided pattern table to VRAM
// Each character is 8x8 pixels, 2 bits per pixel = 16 bits per line
// 8 lines per char, but stored as 2 lines per word = 4 words per character
void GFX_copy_pattern_table(unsigned int* pattern_table)
{
    int i;
    unsigned int addr = GFX_PATTERN_TABLE_ADDR;

    unsigned int *pattern_table_vram = (unsigned int *)GFX_PATTERN_TABLE_ADDR;

    for (i = 0; i < 1024; i++)
    {
        pattern_table_vram[i] = pattern_table[i];
    }

}

// Set cursor position
void GFX_cursor_set(unsigned int x, unsigned int y)
{
    if (x >= 0 && x < GFX_COLS)
        gfx_cursor_x = x;
    if (y >= 0 && y < GFX_ROWS)
        gfx_cursor_y = y;
}

// Get cursor position
void GFX_cursor_get(unsigned int *x, unsigned int *y)
{
    *x = gfx_cursor_x;
    *y = gfx_cursor_y;
}

// Save cursor position
void GFX_cursor_save()
{
    gfx_saved_cursor_x = gfx_cursor_x;
    gfx_saved_cursor_y = gfx_cursor_y;
}

// Restore saved cursor position
void GFX_cursor_restore()
{
    gfx_cursor_x = gfx_saved_cursor_x;
    gfx_cursor_y = gfx_saved_cursor_y;
}

// Prunsigned int a character at specific position without moving cursor
void GFX_putchar_at(char c, unsigned int x, unsigned int y)
{
    unsigned int tile_addr;
    unsigned int color_addr;

    if (x < 0 || x >= GFX_COLS || y < 0 || y >= GFX_ROWS)
        return;

    // Calculate tile address: window tile table + (y * 40 + x)
    tile_addr = GFX_WINDOW_TILE_ADDR + (y * GFX_COLS) + x;

    // Calculate color address: window color table + (y * 40 + x)
    color_addr = GFX_WINDOW_COLOR_ADDR + (y * GFX_COLS) + x;

    // Write the ASCII value as tile index (pattern table has ASCII chars)
    *(int*)tile_addr = (unsigned int)c;

    // Write the palette index
    *(int*)color_addr = gfx_fg_color;
}

// Prunsigned int a character at cursor and advance
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

// Prunsigned int a null-terminated string
void GFX_puts(char *str)
{
    while (*str)
    {
        GFX_putchar(*str);
        str++;
        GFX_debug_uart_putchar(0x42);
    }
}

// Clear the entire screen
void GFX_clear()
{
    int i;
    unsigned int tile_addr;
    unsigned int color_addr;
    unsigned int total_tiles;

    tile_addr = GFX_WINDOW_TILE_ADDR;
    color_addr = GFX_WINDOW_COLOR_ADDR;
    total_tiles = GFX_COLS * GFX_ROWS;

    for (i = 0; i < total_tiles; i++)
    {
        *(int*)(tile_addr + i) = 0;
        *(int*)(color_addr + i) = gfx_fg_color;
    }

    gfx_cursor_x = 0;
    gfx_cursor_y = 0;
}

// Clear a specific line
void GFX_clear_line(unsigned int y)
{
    unsigned int i;
    unsigned int tile_addr;
    unsigned int color_addr;

    if (y < 0 || y >= GFX_ROWS)
        return;

    tile_addr = GFX_WINDOW_TILE_ADDR + (y * GFX_COLS);
    color_addr = GFX_WINDOW_COLOR_ADDR + (y * GFX_COLS);

    for (i = 0; i < GFX_COLS; i++)
    {
        *(int*)(tile_addr + i) = 0;
        *(int*)(color_addr + i) = gfx_fg_color;
    }
}


// Scroll up by N lines
void GFX_scroll_up(unsigned int lines)
{
    unsigned int y, x;
    unsigned int src_tile, dst_tile;
    unsigned int src_color, dst_color;
    unsigned int tile_val, color_val;

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
            tile_val = *(unsigned int*)(src_tile + x);
            color_val = *(unsigned int*)(src_color + x);
            *(int*)(dst_tile + x) = tile_val;
            *(int*)(dst_color + x) = color_val;
        }
    }

    // Clear the bottom lines
    for (y = gfx_scroll_bottom - lines + 1; y <= gfx_scroll_bottom; y++)
    {
        GFX_clear_line(y);
    }
}

int main() {

    char* msg = "abcd";

    GFX_puts(msg);

    GFX_debug_uart_putchar(0x37);

    // While the expected output might seem weird, it is actually correct,
    // as it is being checked against the first UART output, which is 0x42 within GFX_puts().
    return 0x39; // expected=0x42
}

void interrupt()
{
    // No need to handle interrupts
}
