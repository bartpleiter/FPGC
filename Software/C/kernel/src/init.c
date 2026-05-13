/*
 * init.c — BDOS v4 hardware initialization and boot sequence.
 *
 * Boot order: GPU → terminal → timers → UART → Ethernet → USB →
 *   memory allocator → process table → VFS + devices → filesystems
 */
#include "kernel.h"

/* libterm render callback: push cell to GPU window plane. */
static void init_term_render_cb(int x, int y,
                                unsigned char tile, unsigned char palette)
{
    gpu_write_window_tile((unsigned int)x, (unsigned int)y, tile, palette);
}

/* libterm UART mirror callback. */
static void init_term_uart_cb(char c)
{
    uart_putchar(c);
}

static void init_gpu(void)
{
    gpu_clear_vram();
    gpu_load_pattern_table(gpu_default_patterns);
    gpu_load_palette_table(gpu_default_palette);
    gpu_reset_pixel_palette();
}

void kernel_init(void)
{
    set_user_led(1);

    spi_deselect(SPI_FLASH_0);

    /* GPU + terminal */
    init_gpu();
    gpu_set_window_palette(0);
    term_init(TERM_WIDTH, TERM_HEIGHT,
              init_term_render_cb, init_term_uart_cb);
    term_set_palette(PALETTE_WHITE_ON_BLACK);
    kernel_log("BDOS v4 booting\n");

    /* Timers */
    timer_init();
    kernel_log("  timers ok\n");

    /* UART */
    uart_init();
    kernel_log("  uart ok\n");

    /* Networking (Ethernet + FNP) */
    net_init();
    fnp_init();
    kernel_log("  ethernet ok\n");

    /* USB keyboard */
    hid_init();
    kernel_log("  usb ok\n");

    /* Memory allocators */
    kheap_init();
    mem_init();
    kernel_log("  memory ok\n");

    /* Process table */
    proc_init();
    kernel_log("  proc ok\n");

    /* VFS + devices */
    vfs_init();
    dev_init();
    kernel_log("  vfs ok\n");

    /* Set up kernel (pid 0) stdio: fd 0/1/2 → /dev/tty */
    fd_init_stdio();

    /* Filesystems */
    kernel_log("  mounting spi flash\n");
    fs_init();
    kernel_log("  filesystems ok\n");

    set_user_led(0);
    kernel_log("Boot complete\n\n");
}
