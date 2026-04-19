/*
 * Host-side unit tests for /dev/pixpal (BDOS VFS device).
 *
 * Compile:
 *   gcc -O0 -Wall -DVFS_HOST_TEST \
 *       -I Tests/host \
 *       -I Software/C/bdos/include \
 *       Tests/host/test_vfs_pixpal.c Software/C/bdos/vfs.c \
 *       -o /tmp/test_vfs_pixpal
 *
 * Run: ./test_vfs_pixpal — exits 0 on success, nonzero on first failure.
 */

#include "vfs_host_stubs.h"
#include <stdio.h>
#include <string.h>

unsigned int g_fake_pixpal[256];

static bdos_proc_t g_proc;

bdos_proc_t *bdos_proc_current(void)
{
    return &g_proc;
}

static int g_failures = 0;

#define CHECK(cond, msg, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        g_failures++; \
    } \
} while (0)

static void reset(void)
{
    int i;
    for (i = 0; i < 256; i++) g_fake_pixpal[i] = 0;
    bdos_vfs_init();
    /* vfs.c falls back to g_boot_fds until use_proc_tables is called.
     * For these tests either path works; stay on the boot table so we
     * don't touch g_proc. */
}

/* ---------------------------------------------------------------- */

static void test_open_close(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    CHECK(fd >= 3, "open returned %d (expected >=3 since 0..2 are stdio)", fd);
    CHECK(bdos_vfs_close(fd) == 0, "close failed");
}

static void test_open_rejects_creat(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal",
                           BDOS_O_WRONLY | BDOS_O_CREAT);
    CHECK(fd < 0, "open with O_CREAT should fail, got %d", fd);
}

static void test_write_one_entry(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    unsigned int rgb = 0x00FF8040;
    int n = bdos_vfs_write(fd, &rgb, 4);
    CHECK(n == 4, "write returned %d", n);
    CHECK(g_fake_pixpal[0] == 0x00FF8040, "entry 0 = %08x", g_fake_pixpal[0]);
    CHECK(g_fake_pixpal[1] == 0, "entry 1 should be unchanged");
    bdos_vfs_close(fd);
}

static void test_write_autoincrement(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    unsigned int batch[4] = { 0x010203, 0x040506, 0x070809, 0x0A0B0C };
    int n = bdos_vfs_write(fd, batch, 16);
    CHECK(n == 16, "write returned %d", n);
    CHECK(g_fake_pixpal[0] == 0x010203, "entry 0");
    CHECK(g_fake_pixpal[1] == 0x040506, "entry 1");
    CHECK(g_fake_pixpal[2] == 0x070809, "entry 2");
    CHECK(g_fake_pixpal[3] == 0x0A0B0C, "entry 3");
    CHECK(g_fake_pixpal[4] == 0,        "entry 4 unchanged");
    bdos_vfs_close(fd);
}

static void test_lseek_set_then_write(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    int pos = bdos_vfs_lseek(fd, 100 * 4, BDOS_SEEK_SET);
    CHECK(pos == 400, "lseek SET returned %d", pos);
    unsigned int rgb = 0x00ABCDEF;
    bdos_vfs_write(fd, &rgb, 4);
    CHECK(g_fake_pixpal[100] == 0x00ABCDEF, "entry 100");
    CHECK(g_fake_pixpal[0]   == 0,          "entry 0 unchanged");
    bdos_vfs_close(fd);
}

static void test_lseek_end(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    int pos = bdos_vfs_lseek(fd, 0, BDOS_SEEK_END);
    CHECK(pos == 1024, "lseek END returned %d", pos);
    bdos_vfs_close(fd);
}

static void test_full_reload(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    unsigned int batch[256];
    int i;
    for (i = 0; i < 256; i++) batch[i] = (unsigned int)(i * 0x10101);
    int n = bdos_vfs_write(fd, batch, 256 * 4);
    CHECK(n == 1024, "write returned %d", n);
    for (i = 0; i < 256; i++) {
        CHECK(g_fake_pixpal[i] == (unsigned int)(i * 0x10101),
              "entry %d = %08x", i, g_fake_pixpal[i]);
    }
    bdos_vfs_close(fd);
}

static void test_write_past_end_fails(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    bdos_vfs_lseek(fd, 1020, BDOS_SEEK_SET);  /* 4 bytes left */
    unsigned int batch[2] = { 1, 2 };
    int n = bdos_vfs_write(fd, batch, 8);
    CHECK(n == -1, "write past end should fail, got %d", n);
    bdos_vfs_close(fd);
}

static void test_misaligned_rejected(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    char misaligned[3] = { 1, 2, 3 };
    int n = bdos_vfs_write(fd, misaligned, 3);
    CHECK(n == -1, "misaligned write should fail, got %d", n);
    /* Misaligned cursor */
    bdos_vfs_lseek(fd, 2, BDOS_SEEK_SET);
    unsigned int rgb = 0xAA;
    n = bdos_vfs_write(fd, &rgb, 4);
    CHECK(n == -1, "misaligned cursor write should fail, got %d", n);
    bdos_vfs_close(fd);
}

static void test_read_back(void)
{
    reset();
    g_fake_pixpal[5] = 0x00DEADBE;
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_RDONLY);
    bdos_vfs_lseek(fd, 5 * 4, BDOS_SEEK_SET);
    unsigned int v = 0;
    int n = bdos_vfs_read(fd, &v, 4);
    CHECK(n == 4, "read returned %d", n);
    CHECK(v == 0x00DEADBE, "read value = %08x", v);
    bdos_vfs_close(fd);
}

static void test_read_eof(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_RDONLY);
    bdos_vfs_lseek(fd, 1024, BDOS_SEEK_SET);
    unsigned int v;
    int n = bdos_vfs_read(fd, &v, 4);
    CHECK(n == 0, "read at EOF should return 0, got %d", n);
    bdos_vfs_close(fd);
}

static void test_top_byte_masked(void)
{
    reset();
    int fd = bdos_vfs_open("/dev/pixpal", BDOS_O_WRONLY);
    unsigned int rgb = 0xFFAABBCC;       /* top byte set */
    bdos_vfs_write(fd, &rgb, 4);
    CHECK(g_fake_pixpal[0] == 0x00AABBCC,
          "top byte should be masked, got %08x", g_fake_pixpal[0]);
    bdos_vfs_close(fd);
}

/* ---------------------------------------------------------------- */

int main(void)
{
    test_open_close();
    test_open_rejects_creat();
    test_write_one_entry();
    test_write_autoincrement();
    test_lseek_set_then_write();
    test_lseek_end();
    test_full_reload();
    test_write_past_end_fails();
    test_misaligned_rejected();
    test_read_back();
    test_read_eof();
    test_top_byte_masked();

    if (g_failures == 0) {
        printf("All /dev/pixpal tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d failures.\n", g_failures);
    return 1;
}
