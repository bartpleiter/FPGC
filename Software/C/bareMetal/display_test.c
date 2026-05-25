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
#define VRAM32_BASE         0x1E400000  /* 1056 words: patterns + palettes */
#define VRAM8_BASE          0x1E800000  /* 8194 bytes: tile + color maps */
#define WIN_TILE_BASE       0x1E804000  /* Window tile map (40x30) */
#define WIN_COLOR_BASE      0x1E806000  /* Window color map (40x30) */
#define PATTERN_TABLE       0x1E400000  /* Tile patterns (256 tiles x 4 words) */
#define PALETTE_TABLE       0x1E401000  /* Tile palettes (32 palettes x 1 word) */
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

/* ---- 5x5 font for letters H, E, L, O (1-bit, packed into rows) ---- */
/* Each letter is 5 rows of 5 pixels. Stored as 5 bytes, MSB-first.     */
/* Pattern format: 2-bit per pixel, 8 pixels wide, 8 rows.              */
/* We'll use a simple 5x5 font centered in the 8x8 tile.                */

/* Write a single 8x8 tile pattern into VRAM32.                         */
/* tile_index: which tile (0-255)                                        */
/* rows: array of 8 bytes, each byte has 8 pixels (1-bit, MSB=left)      */
void
write_tile_pattern(int tile_index, int *rows)
{
    int word_addr;
    int row;
    int word;
    int px;
    int bit;
    int half;

    /* Each tile = 4 words in VRAM32. Each word = 2 rows of 8 pixels x 2 bits */
    word_addr = PATTERN_TABLE + tile_index * 4 * 4;

    for (row = 0; row < 8; row = row + 2) {
        /* Pack two rows into one 32-bit word */
        /* Even row in upper 16 bits, odd row in lower 16 bits */
        /* Each pixel: 2 bits. Pixel 0 at MSB end. */
        /* We use pattern_bits 01 for "on" (foreground) */
        word = 0;

        /* Even row (upper 16 bits) */
        half = 0;
        for (px = 0; px < 8; px++) {
            bit = (rows[row] >> (7 - px)) & 1;
            if (bit)
                half = half | (1 << ((7 - px) * 2));
        }
        word = half << 16;

        /* Odd row (lower 16 bits) */
        half = 0;
        for (px = 0; px < 8; px++) {
            bit = (rows[row + 1] >> (7 - px)) & 1;
            if (bit)
                half = half | (1 << ((7 - px) * 2));
        }
        word = word | half;

        store(word_addr + (row / 2) * 4, word);
    }
}

/* ---- Define letter tile patterns ---- */
void
define_font_tiles(void)
{
    int rows[8];

    /* Tile 1: 'H' */
    rows[0] = 0x88; /* 10001000 */
    rows[1] = 0x88;
    rows[2] = 0x88;
    rows[3] = 0xF8; /* 11111000 */
    rows[4] = 0x88;
    rows[5] = 0x88;
    rows[6] = 0x88;
    rows[7] = 0x00;
    write_tile_pattern(1, rows);

    /* Tile 2: 'E' */
    rows[0] = 0xF8; /* 11111000 */
    rows[1] = 0x80;
    rows[2] = 0x80;
    rows[3] = 0xF0; /* 11110000 */
    rows[4] = 0x80;
    rows[5] = 0x80;
    rows[6] = 0xF8;
    rows[7] = 0x00;
    write_tile_pattern(2, rows);

    /* Tile 3: 'L' */
    rows[0] = 0x80; /* 10000000 */
    rows[1] = 0x80;
    rows[2] = 0x80;
    rows[3] = 0x80;
    rows[4] = 0x80;
    rows[5] = 0x80;
    rows[6] = 0xF8;
    rows[7] = 0x00;
    write_tile_pattern(3, rows);

    /* Tile 4: 'O' */
    rows[0] = 0x70; /* 01110000 */
    rows[1] = 0x88;
    rows[2] = 0x88;
    rows[3] = 0x88;
    rows[4] = 0x88;
    rows[5] = 0x88;
    rows[6] = 0x70;
    rows[7] = 0x00;
    write_tile_pattern(4, rows);
}

/* ---- Set up window tile palette ---- */
void
setup_tile_palette(void)
{
    int palette_word;

    /* Palette 0: used for transparent tiles (all zeros = transparent) */
    store(PALETTE_TABLE, 0x00000000);

    /* Palette 1: white text on transparent background */
    /* Color 0 (bg) = 0x00 (transparent: RRRGGGBB=0, high byte=0) */
    /* Color 1 (fg) = 0xFF (white: RRR=7, GGG=7, BB=3) */
    /* Color 2      = 0x00 */
    /* Color 3      = 0x00 */
    /* Packed: [31:24]=color0, [23:16]=color1, [15:8]=color2, [7:0]=color3 */
    palette_word = 0x00FF0000;
    store(PALETTE_TABLE + 1 * 4, palette_word);
}

/* ---- Place "HELLO" in the center of the window tile layer ---- */
void
draw_hello(void)
{
    int tile_y;
    int tile_x;
    int offset;

    /* Center: row 15, starting at column 17 (40 cols, 5 letters = start at 17) */
    tile_y = 15;
    tile_x = 17;

    /* H */
    offset = tile_y * 40 + tile_x;
    store(WIN_TILE_BASE + offset, 1);
    store(WIN_COLOR_BASE + offset, 1);

    /* E */
    offset = tile_y * 40 + tile_x + 1;
    store(WIN_TILE_BASE + offset, 2);
    store(WIN_COLOR_BASE + offset, 1);

    /* L */
    offset = tile_y * 40 + tile_x + 2;
    store(WIN_TILE_BASE + offset, 3);
    store(WIN_COLOR_BASE + offset, 1);

    /* L */
    offset = tile_y * 40 + tile_x + 3;
    store(WIN_TILE_BASE + offset, 3);
    store(WIN_COLOR_BASE + offset, 1);

    /* O */
    offset = tile_y * 40 + tile_x + 4;
    store(WIN_TILE_BASE + offset, 4);
    store(WIN_COLOR_BASE + offset, 1);
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

    /* Step 0: Clear window tile layer (remove bootloader splash remnants) */
    uart_puts("Clearing window tiles...\n");
    for (i = 0; i < 1200; i++) {
        store(WIN_TILE_BASE + i, 0);
        store(WIN_COLOR_BASE + i, 0);
    }
    /* Clear palette entry 0 in VRAM32 (ensure transparency) */
    store(PALETTE_TABLE, 0x00000000);

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
