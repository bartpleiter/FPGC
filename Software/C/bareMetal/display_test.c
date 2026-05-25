/*
 * display_test.c — Bare-metal SPI display + window tile layer test
 *
 * Tests three things:
 *   1. Pixel framebuffer (VRAMPX) — fills with a color gradient
 *   2. Pixel palette — sets up default RRRGGGBB palette
 *   3. Window tile layer — draws "HELLO" text in the center
 *
 * Run with: make run-c-baremetal-uart file=display_test
 */

/* ---- MMIO addresses (from fpgc.h) ---- */
#define VRAMPX_BASE         0x1EC00000  /* 76800 bytes: 320x240 indexed */
#define PIXEL_PALETTE_BASE  0x1EC80000  /* 256 entries x 4 bytes (RGB24) */
#define WIN_TILE_BASE       0x1E804000  /* Window tile map: word-addressed */
#define WIN_COLOR_BASE      0x1E806000  /* Window color map: word-addressed */
#define PATTERN_TABLE       0x1E400000  /* Tile patterns: word-addressed */
#define PALETTE_TABLE       0x1E401000  /* Tile palettes: word-addressed */
#define UART_TX             0x1C000000
#define USER_LED            0x1C00006C

/* ---- Helpers ---- */
void
store(int addr, int val)
{
    __builtin_store((int *)addr, val);
}

int
load(int addr)
{
    return __builtin_load((int *)addr);
}

void
uart_putc(int c)
{
    store(UART_TX, c);
}

void
uart_puts(char *s)
{
    while (*s) {
        uart_putc(*s);
        s = s + 1;
    }
}

void
delay(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        /* empty */
    }
}

/* ---- Pixel palette: default RRRGGGBB -> RGB24 ---- */
void
init_pixel_palette(void)
{
    int i;
    int r3;
    int g3;
    int b2;
    int r;
    int g;
    int b;
    int rgb24;

    for (i = 0; i < 256; i++) {
        r3 = (i >> 5) & 7;
        g3 = (i >> 2) & 7;
        b2 = i & 3;
        r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
        g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
        b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
        rgb24 = (r << 16) | (g << 8) | b;
        store(PIXEL_PALETTE_BASE + i * 4, rgb24);
    }
}

/* ---- Fill VRAMPX with a gradient ---- */
void
fill_gradient(void)
{
    int x;
    int y;
    int pixel;
    int addr;

    for (y = 0; y < 240; y++) {
        for (x = 0; x < 320; x++) {
            /* RRRGGGBB: R from y, G from x, B fixed */
            pixel = ((y >> 2) << 5) | ((x >> 4) << 2) | 1;
            addr = VRAMPX_BASE + y * 320 + x;
            store(addr, pixel & 0xFF);
        }
    }
}

/*
 * Load font patterns for H, E, L, O into the pattern table.
 * Uses the exact data from gpu_data_ascii.c (known working).
 * Each tile = 4 words at PATTERN_TABLE + ascii_code * 16.
 * VRAM32 is word-addressed: each word at addr + i*4.
 */
void
define_font_tiles(void)
{
    int base;

    /* 'E' (ASCII 69) — 4 words */
    base = PATTERN_TABLE + 69 * 16;
    store(base + 0, 0xFFFC3C0C);
    store(base + 4, 0x3CC03FC0);
    store(base + 8, 0x3CC03C0C);
    store(base + 12, 0xFFFC0000);

    /* 'H' (ASCII 72) — 4 words */
    base = PATTERN_TABLE + 72 * 16;
    store(base + 0, 0xF0F0F0F0);
    store(base + 4, 0xF0F0FFF0);
    store(base + 8, 0xF0F0F0F0);
    store(base + 12, 0xF0F00000);

    /* 'L' (ASCII 76) — 4 words */
    base = PATTERN_TABLE + 76 * 16;
    store(base + 0, 0xFF003C00);
    store(base + 4, 0x3C003C00);
    store(base + 8, 0x3C0C3C3C);
    store(base + 12, 0xFFFC0000);

    /* 'O' (ASCII 79) — 4 words */
    base = PATTERN_TABLE + 79 * 16;
    store(base + 0, 0x0FC03CF0);
    store(base + 4, 0xF03CF03C);
    store(base + 8, 0xF03C3CF0);
    store(base + 12, 0x0FC00000);
}

/*
 * Set up tile palette 0 (white on transparent black).
 * Format: [31:24]=color0, [23:16]=color1, [15:8]=color2, [7:0]=color3
 * Pattern 2'b00 → color0 (bg), 2'b11 → color3 (fg in default font).
 * 0x0000FFFF: color0=0x00 (black/transparent), color3=0xFF (white).
 */
void
setup_tile_palette(void)
{
    /* Palette 0: white on transparent black (same as gpu_default_palette[0]) */
    store(PALETTE_TABLE + 0 * 4, 0x0000FFFF);
}

/*
 * Place "HELLO" in the center of the window tile layer.
 * VRAM8 is word-addressed: each entry at addr + idx * 4.
 * Tile indices use ASCII codes matching the pattern table.
 */
void
draw_hello(void)
{
    int tile_y;
    int tile_x;
    int idx;

    /* Center: row 15, starting at column 17 */
    tile_y = 15;
    tile_x = 17;

    /* H */
    idx = tile_y * 40 + tile_x;
    store(WIN_TILE_BASE + idx * 4, 72);
    store(WIN_COLOR_BASE + idx * 4, 0);

    /* E */
    idx = tile_y * 40 + tile_x + 1;
    store(WIN_TILE_BASE + idx * 4, 69);
    store(WIN_COLOR_BASE + idx * 4, 0);

    /* L */
    idx = tile_y * 40 + tile_x + 2;
    store(WIN_TILE_BASE + idx * 4, 76);
    store(WIN_COLOR_BASE + idx * 4, 0);

    /* L */
    idx = tile_y * 40 + tile_x + 3;
    store(WIN_TILE_BASE + idx * 4, 76);
    store(WIN_COLOR_BASE + idx * 4, 0);

    /* O */
    idx = tile_y * 40 + tile_x + 4;
    store(WIN_TILE_BASE + idx * 4, 79);
    store(WIN_COLOR_BASE + idx * 4, 0);
}

/*
 * Place tiles in all four corners for alignment validation.
 * H = top-left (0,0), E = top-right (39,0),
 * L = bottom-left (0,29), O = bottom-right (39,29).
 * Tile grid is 40 columns × 30 rows.
 */
void
draw_corners(void)
{
    int idx;

    /* Top-left corner: H at tile (0, 0) */
    idx = 0 * 40 + 0;
    store(WIN_TILE_BASE + idx * 4, 72);
    store(WIN_COLOR_BASE + idx * 4, 0);

    /* Top-right corner: E at tile (39, 0) */
    idx = 0 * 40 + 39;
    store(WIN_TILE_BASE + idx * 4, 69);
    store(WIN_COLOR_BASE + idx * 4, 0);

    /* Bottom-left corner: L at tile (0, 29) */
    idx = 29 * 40 + 0;
    store(WIN_TILE_BASE + idx * 4, 76);
    store(WIN_COLOR_BASE + idx * 4, 0);

    /* Bottom-right corner: O at tile (39, 29) */
    idx = 29 * 40 + 39;
    store(WIN_TILE_BASE + idx * 4, 79);
    store(WIN_COLOR_BASE + idx * 4, 0);
}

/* ---- Interrupt handler (required by crt0) ---- */
void
interrupt(void)
{
    /* Do nothing */
}

/* ---- Main ---- */
int
main(void)
{
    int i;

    uart_puts("Display test starting\n");

    /* Turn on user LED */
    store(USER_LED, 1);

    /* Step 0: Clear ALL tile layer data (remove bootloader splash remnants) */
    uart_puts("Clearing VRAM...\n");

    /* Clear VRAM8 tile/color maps (word-addressed) */
    for (i = 0; i < 1200; i++) {
        store(WIN_TILE_BASE + i * 4, 0);
        store(WIN_COLOR_BASE + i * 4, 0);
    }

    /* Clear ALL VRAM32 pattern data (256 tiles × 4 words = 1024 words) */
    for (i = 0; i < 1024; i++) {
        store(PATTERN_TABLE + i * 4, 0);
    }

    /* Clear ALL VRAM32 palette data (32 palettes × 1 word = 32 words) */
    for (i = 0; i < 32; i++) {
        store(PALETTE_TABLE + i * 4, 0);
    }

    /* Step 1: Set up pixel palette */
    uart_puts("Setting pixel palette...\n");
    init_pixel_palette();

    /* Step 2: Fill VRAMPX with gradient */
    uart_puts("Filling pixel gradient...\n");
    fill_gradient();

    uart_puts("Gradient displayed. Waiting 5 seconds before HELLO...\n");
    delay(50000000);

    /* Step 3: Define font tile patterns in VRAM32 */
    uart_puts("Defining font tiles...\n");
    define_font_tiles();

    /* Step 4: Set up tile palette in VRAM32 */
    uart_puts("Setting tile palette...\n");
    setup_tile_palette();

    /* Step 5: Draw "HELLO" using window tile layer */
    uart_puts("Drawing HELLO...\n");
    draw_hello();

    /* Step 6: Draw corner tiles for alignment validation */
    uart_puts("Drawing corner tiles...\n");
    draw_corners();

    uart_puts("Display test complete!\n");

    /* Blink LED to show we're alive */
    while (1) {
        store(USER_LED, 1);
        delay(5000000);
        store(USER_LED, 0);
        delay(5000000);
    }

    return 0;
}
