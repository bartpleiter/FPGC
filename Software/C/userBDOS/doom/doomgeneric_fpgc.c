/*
 * doomgeneric_fpgc.c — FPGC platform implementation for doomgeneric.
 *
 * Implements the 5 required platform functions plus main().
 * Uses BDOS syscalls for input, timing, and display.
 */

/* Include syscall.h first, before Doom headers redefine KEY_F* etc. */
#include <time.h>
#include <stdio.h>
#include <syscall.h>

#include "config.h"

/* BDOS syscall numbers and key state bits — use raw numbers to avoid
 * name collisions with Doom's own KEY_* defines. */
extern int syscall(int num, int a1, int a2, int a3);

/* BDOS syscall wrappers using raw numbers */
static int bdos_read_key(void)       { return syscall(2, 0, 0, 0); }
static int bdos_key_available(void)  { return syscall(3, 0, 0, 0); }
static int bdos_get_key_state(void)  { return syscall(25, 0, 0, 0); }
static void bdos_set_pixel_palette(int idx, int rgb24) { syscall(26, idx, rgb24, 0); }

#include "doomgeneric.h"
#include "doomkeys.h"
#include "doomtype.h"
#include "i_video.h"

/* Pixel framebuffer base address (320x240, 8-bit indexed) */
#define PIXEL_FB   ((unsigned char *)0x1EC00000)
#define FB_WIDTH   320
#define FB_HEIGHT  240

/* Doom renders at 320x200 — we copy to the top of the 320x240 FB */
#define DOOM_WIDTH  320
#define DOOM_HEIGHT 200

/* Key event ring buffer */
#define KEY_QUEUE_SIZE 32
static struct {
    unsigned char key;
    int pressed;
} key_queue[KEY_QUEUE_SIZE];
static int key_queue_head = 0;
static int key_queue_tail = 0;

/* Previous key state for edge detection */
static int prev_key_state = 0;

/* Microsecond counter base for tick calculation */
static unsigned int tick_base = 0;

/* Forward declarations */
static void fpgc_poll_keys(void);
static unsigned char translate_bdos_key(int bdos_key);

/* ---- Required platform functions ---- */

void DG_Init(void)
{
    /* Record base time */
    tick_base = get_micros();

    /* Clear the pixel framebuffer (word-addressed: 4 bytes per pixel) */
    int i;
    for (i = 0; i < FB_WIDTH * FB_HEIGHT; i++)
        __builtin_storeb(0x1EC00000 + i * 4, 0);
}

void DG_DrawFrame(void)
{
    /* Copy Doom's 320x200 screen buffer to FPGC's pixel framebuffer.
     * DG_ScreenBuffer is 8-bit palette indices (CMAP256 mode).
     * The pixel FB is 8-bit indexed, word-addressed from CPU:
     * VRAMPX address = (byte_addr - 0x1EC00000) >> 2, so each
     * pixel occupies 4 bytes of CPU address space. */
    unsigned char *src = (unsigned char *)DG_ScreenBuffer;
    int i;

    for (i = 0; i < DOOM_WIDTH * DOOM_HEIGHT; i++)
        __builtin_storeb(0x1EC00000 + i * 4, src[i]);

    /* Update the FPGC palette from Doom's colors array */
#ifdef CMAP256
    extern boolean palette_changed;
    extern struct color colors[256];
    if (palette_changed) {
        for (i = 0; i < 256; i++) {
            int rgb24 = (colors[i].r << 16) | (colors[i].g << 8) | colors[i].b;
            bdos_set_pixel_palette(i, rgb24);
        }
        palette_changed = 0;
    }
#endif
}

void DG_SleepMs(unsigned int ms)
{
    unsigned int start = get_micros();
    unsigned int target = ms * 1000;
    while ((get_micros() - start) < target)
        ;
}

unsigned int DG_GetTicksMs(void)
{
    return (get_micros() - tick_base) / 1000;
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    /* Poll for new key events */
    fpgc_poll_keys();

    /* Return next event from queue */
    if (key_queue_head != key_queue_tail) {
        *pressed = key_queue[key_queue_tail].pressed;
        *key = key_queue[key_queue_tail].key;
        key_queue_tail = (key_queue_tail + 1) % KEY_QUEUE_SIZE;
        return 1;
    }
    return 0;
}

void DG_SetWindowTitle(const char *title)
{
    /* No window title on FPGC */
}

/* ---- Key polling ---- */

static void enqueue_key(unsigned char doom_key, int pressed)
{
    int next = (key_queue_head + 1) % KEY_QUEUE_SIZE;
    if (next == key_queue_tail)
        return;  /* queue full */
    key_queue[key_queue_head].key = doom_key;
    key_queue[key_queue_head].pressed = pressed;
    key_queue_head = next;
}

/* Map key state bitmap bits to Doom key codes */
struct keymap_entry {
    int state_bit;
    unsigned char doom_key;
};

static struct keymap_entry keymap[] = {
    { 0x0010, KEY_UPARROW },    /* KEYSTATE_UP */
    { 0x0020, KEY_DOWNARROW },  /* KEYSTATE_DOWN */
    { 0x0040, KEY_LEFTARROW },  /* KEYSTATE_LEFT */
    { 0x0080, KEY_RIGHTARROW }, /* KEYSTATE_RIGHT */
    { 0x0001, KEY_UPARROW },   /* KEYSTATE_W — forward */
    { 0x0002, KEY_STRAFE_L },  /* KEYSTATE_A — strafe left */
    { 0x0004, KEY_DOWNARROW },  /* KEYSTATE_S — backward */
    { 0x0008, KEY_STRAFE_R },  /* KEYSTATE_D — strafe right */
    { 0x0100, KEY_USE },        /* KEYSTATE_SPACE — use */
    { 0x0200, KEY_RSHIFT },     /* KEYSTATE_SHIFT — run */
    { 0x0400, KEY_FIRE },       /* KEYSTATE_CTRL — fire */
    { 0x0800, KEY_ESCAPE },     /* KEYSTATE_ESCAPE */
    { 0x1000, KEY_ENTER },      /* KEYSTATE_E — enter/use */
    { 0x2000, KEY_TAB },        /* KEYSTATE_Q — automap */
};

#define KEYMAP_COUNT (sizeof(keymap) / sizeof(keymap[0]))

static void fpgc_poll_keys(void)
{
    int state = bdos_get_key_state();
    int changed = state ^ prev_key_state;
    int i;

    for (i = 0; i < (int)KEYMAP_COUNT; i++) {
        if (changed & keymap[i].state_bit) {
            int pressed = (state & keymap[i].state_bit) ? 1 : 0;
            enqueue_key(keymap[i].doom_key, pressed);
        }
    }

    prev_key_state = state;

    /* Also check character-level keys (for menu: enter, numbers, etc.)
     * Only process first queued key per poll to avoid flooding */
    if (bdos_key_available()) {
        int k = bdos_read_key();
        unsigned char dk = translate_bdos_key(k);
        if (dk != 0) {
            enqueue_key(dk, 1);
            enqueue_key(dk, 0);
        }
    }
}

static unsigned char translate_bdos_key(int bdos_key)
{
    /* ASCII keys pass through directly */
    if (bdos_key >= 0x20 && bdos_key < 0x7f)
        return (unsigned char)bdos_key;

    /* Special keys */
    if (bdos_key == 0x0D || bdos_key == '\n')
        return KEY_ENTER;
    if (bdos_key == 0x1B)
        return KEY_ESCAPE;
    if (bdos_key == 0x09)
        return KEY_TAB;
    if (bdos_key == 0x08 || bdos_key == 0x7F)
        return KEY_BACKSPACE;

    /* BDOS special key codes — arrow keys are handled by key_state bitmap,
     * so only map non-bitmap keys here to avoid duplicate events */
    switch (bdos_key) {
    case 0x10B: return KEY_F1;
    case 0x10C: return KEY_F2;
    case 0x10D: return KEY_F3;
    case 0x10E: return KEY_F4;
    case 0x10F: return KEY_F5;
    case 0x110: return KEY_F6;
    case 0x111: return KEY_F7;
    case 0x112: return KEY_F8;
    case 0x113: return KEY_F9;
    case 0x114: return KEY_F10;
    case 0x115: return KEY_F11;
    case 0x116: return KEY_F12;
    default:    return 0;
    }
}

/* ---- Entry point ---- */

/* argv must be static so it survives across all Doom function calls.
 * A stack-local array in main() can be clobbered by deep call chains. */
static char *doom_argv[] = { "doom", "-iwad", "/data/doom/doom1.wad" };

int main(void)
{
    /* Clear terminal before switching to pixel framebuffer mode */
    sys_term_clear();

    printf("DOOM: main() entered\n");

    printf("DOOM: calling doomgeneric_Create\n");
    doomgeneric_Create(3, doom_argv);

    printf("DOOM: entering main loop\n");

    /* Main game loop */
    for (;;) {
        doomgeneric_Tick();
    }

    return 0;
}
