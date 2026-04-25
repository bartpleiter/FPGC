// dmabench.c -- compare CPU vs DMA fullscreen blit speed for VRAMPX.
//
// Allocates a 76800-byte SDRAM scratch buffer (32-byte aligned), fills it
// with a test pattern, and times two equivalent operations:
//   1. CPU blit:   for-loop with __builtin_storeb to PIXEL_FB_ADDR
//   2. DMA blit:   dma_blit_to_vram(PIXEL_FB_ADDR, scratch, 76800)
//
// Repeats N times and prints the per-frame and total microsecond counts.

#include <syscall.h>
#include <time.h>
#include <plot.h>   // PIXEL_FB_ADDR
#include <dma.h>

#define W       320
#define H       240
#define BYTES   (W * H)   /* 76800 */
#define REPS    30

static void
print_uint(unsigned int n)
{
    char buf[16];
    char tmp[12];
    int  i = 0;
    int  j;

    if (n == 0)
    {
        tmp[i++] = '0';
    }
    else
    {
        while (n > 0)
        {
            tmp[i++] = '0' + (int)(n % 10U);
            n = n / 10U;
        }
    }
    j = 0;
    while (i > 0)
    {
        buf[j++] = tmp[--i];
    }
    buf[j] = 0;
    sys_putstr(buf);
}

int
main(void)
{
    unsigned int  t0;
    unsigned int  cpu_us;
    unsigned int  dma_us;
    int           rep;
    int           i;
    int           rc;

    /* Allocate scratch buffer with room to round up to 32-byte alignment. */
    unsigned char *raw = (unsigned char *)sys_heap_alloc(BYTES + 32);
    if (raw == 0)
    {
        sys_putstr("dmabench: allocation failed\n");
        return 1;
    }
    unsigned int   scratch_addr = ((unsigned int)raw + 31U) & ~31U;
    unsigned char *scratch      = (unsigned char *)scratch_addr;

    /* Fill the scratch with a diagonal-stripe pattern. */
    for (i = 0; i < BYTES; i++)
    {
        scratch[i] = (unsigned char)(i & 0xFF);
    }

    sys_putstr("dmabench: ");
    print_uint((unsigned int)REPS);
    sys_putstr(" frames of ");
    print_uint((unsigned int)BYTES);
    sys_putstr(" bytes\n");

    /* ---- CPU blit timing ---- */
    t0 = get_micros();
    for (rep = 0; rep < REPS; rep++)
    {
        for (i = 0; i < BYTES; i++)
        {
            __builtin_storeb(PIXEL_FB_ADDR + i, scratch[i]);
        }
    }
    cpu_us = get_micros() - t0;

    sys_putstr("  CPU loop: total ");
    print_uint(cpu_us);
    sys_putstr(" us, per frame ");
    print_uint(cpu_us / (unsigned int)REPS);
    sys_putstr(" us\n");

    /* ---- DMA blit timing ---- */
    t0 = get_micros();
    for (rep = 0; rep < REPS; rep++)
    {
        rc = dma_blit_to_vram(PIXEL_FB_ADDR, scratch_addr,
                              (unsigned int)BYTES);
        if (rc != 0)
        {
            sys_putstr("  DMA error at rep ");
            print_uint((unsigned int)rep);
            sys_putstr("\n");
            return 1;
        }
    }
    dma_us = get_micros() - t0;

    sys_putstr("  DMA blit: total ");
    print_uint(dma_us);
    sys_putstr(" us, per frame ");
    print_uint(dma_us / (unsigned int)REPS);
    sys_putstr(" us\n");

    /* ---- Speedup ---- */
    if (dma_us > 0)
    {
        sys_putstr("  Speedup x100: ");
        print_uint((cpu_us * 100U) / dma_us);
        sys_putstr("\n");
    }

    return 0;
}
