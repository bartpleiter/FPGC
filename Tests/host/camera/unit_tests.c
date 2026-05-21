/*
 * FPGC-Camera — Unit Tests
 *
 * Validates individual pipeline stages with known inputs/outputs.
 * Returns 0 on all pass, 1 on any failure.
 */

#include <stdio.h>
#include <string.h>
#include "image_proc.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(msg, expected, actual) do { \
    tests_run++; \
    if ((expected) != (actual)) { \
        printf("FAIL: %s: expected %d, got %d\n", msg, (int)(expected), (int)(actual)); \
    } else { \
        tests_passed++; \
    } \
} while(0)

/* --- Downsample tests --------------------------------------------------- */

static void test_downsample_uniform(void)
{
    /* 4×4 uniform image → 2×2, all same value */
    u8 in[16], out[4];
    int i;
    for (i = 0; i < 16; i++) in[i] = 100;
    downsample_2x2(in, out, 4, 4);
    ASSERT_EQ("ds_uniform[0]", 100, out[0]);
    ASSERT_EQ("ds_uniform[1]", 100, out[1]);
    ASSERT_EQ("ds_uniform[2]", 100, out[2]);
    ASSERT_EQ("ds_uniform[3]", 100, out[3]);
}

static void test_downsample_average(void)
{
    /* 4×2 image with known values */
    u8 in[8] = { 0, 100, 200, 40,
                 50, 150, 0,   10 };
    u8 out[2];
    downsample_2x2(in, out, 4, 2);
    /* Block (0,0): (0+100+50+150)/4 = 75 */
    ASSERT_EQ("ds_avg[0]", 75, out[0]);
    /* Block (1,0): (200+40+0+10)/4 = 62 */
    ASSERT_EQ("ds_avg[1]", 62, out[1]);
}

/* --- Auto-contrast tests ------------------------------------------------ */

static void test_autocontrast_stretch(void)
{
    /* Input range [50, 200] should stretch to [0, 255] */
    u8 buf[4] = { 50, 200, 125, 100 };
    auto_contrast(buf, 4, 1);
    ASSERT_EQ("ac_lo", 0, buf[0]);
    ASSERT_EQ("ac_hi", 255, buf[1]);
    /* 125: (125-50)*255/150 = 127 */
    ASSERT_EQ("ac_mid", 127, buf[2]);
    /* 100: (100-50)*255/150 = 85 */
    ASSERT_EQ("ac_100", 85, buf[3]);
}

static void test_autocontrast_noop(void)
{
    /* Input already [0, 255] — should not change */
    u8 buf[3] = { 0, 128, 255 };
    auto_contrast(buf, 3, 1);
    ASSERT_EQ("ac_noop_lo", 0, buf[0]);
    ASSERT_EQ("ac_noop_mid", 128, buf[1]);
    ASSERT_EQ("ac_noop_hi", 255, buf[2]);
}

static void test_autocontrast_flat(void)
{
    /* All same value — should remain unchanged */
    u8 buf[4] = { 100, 100, 100, 100 };
    auto_contrast(buf, 4, 1);
    ASSERT_EQ("ac_flat", 100, buf[0]);
}

/* --- Dither tests ------------------------------------------------------- */

static void test_dither_black(void)
{
    /* All-black input should produce all shade 0 */
    u8 in[16], out[16];
    int i;
    memset(in, 0, 16);
    dither_4x4(in, out, 4, 4);
    for (i = 0; i < 16; i++) {
        ASSERT_EQ("dith_black", 0, out[i]);
    }
}

static void test_dither_white(void)
{
    /* All-white input should produce all shade 3 */
    u8 in[16], out[16];
    int i;
    memset(in, 255, 16);
    dither_4x4(in, out, 4, 4);
    for (i = 0; i < 16; i++) {
        ASSERT_EQ("dith_white", 3, out[i]);
    }
}

static void test_dither_range(void)
{
    /* Output values should always be 0-3 */
    u8 in[16], out[16];
    int v, i;
    for (v = 0; v < 256; v++) {
        memset(in, v, 16);
        dither_4x4(in, out, 4, 4);
        for (i = 0; i < 16; i++) {
            tests_run++;
            if (out[i] > 3) {
                printf("FAIL: dith_range v=%d i=%d: got %d\n", v, i, out[i]);
            } else {
                tests_passed++;
            }
        }
    }
}

static void test_dither_monotonic(void)
{
    /* For any fixed position, increasing input should never decrease shade */
    u8 in_a[16], in_b[16], out_a[16], out_b[16];
    int v, i;
    for (v = 0; v < 255; v++) {
        memset(in_a, v, 16);
        memset(in_b, v + 1, 16);
        dither_4x4(in_a, out_a, 4, 4);
        dither_4x4(in_b, out_b, 4, 4);
        for (i = 0; i < 16; i++) {
            tests_run++;
            if (out_b[i] < out_a[i]) {
                printf("FAIL: dith_mono v=%d i=%d: %d > %d\n",
                       v, i, out_a[i], out_b[i]);
            } else {
                tests_passed++;
            }
        }
    }
}

/* --- Scale tests -------------------------------------------------------- */

static void test_scale_2x(void)
{
    /* 2×2 input → 4×4 output */
    u8 in[4] = { 0, 1, 2, 3 };
    u8 out[16];
    scale_2x(in, out, 2, 2);
    /* Row 0: 0 0 1 1 */
    ASSERT_EQ("sc_00", 0, out[0]);
    ASSERT_EQ("sc_01", 0, out[1]);
    ASSERT_EQ("sc_02", 1, out[2]);
    ASSERT_EQ("sc_03", 1, out[3]);
    /* Row 1: 0 0 1 1 */
    ASSERT_EQ("sc_10", 0, out[4]);
    ASSERT_EQ("sc_11", 0, out[5]);
    ASSERT_EQ("sc_12", 1, out[6]);
    ASSERT_EQ("sc_13", 1, out[7]);
    /* Row 2: 2 2 3 3 */
    ASSERT_EQ("sc_20", 2, out[8]);
    ASSERT_EQ("sc_21", 2, out[9]);
    ASSERT_EQ("sc_22", 3, out[10]);
    ASSERT_EQ("sc_23", 3, out[11]);
    /* Row 3: 2 2 3 3 */
    ASSERT_EQ("sc_30", 2, out[12]);
    ASSERT_EQ("sc_31", 2, out[13]);
    ASSERT_EQ("sc_32", 3, out[14]);
    ASSERT_EQ("sc_33", 3, out[15]);
}

/* --- Main --------------------------------------------------------------- */

int main(void)
{
    printf("FPGC-Camera unit tests\n");
    printf("======================\n\n");

    test_downsample_uniform();
    test_downsample_average();
    test_autocontrast_stretch();
    test_autocontrast_noop();
    test_autocontrast_flat();
    test_dither_black();
    test_dither_white();
    test_dither_range();
    test_dither_monotonic();
    test_scale_2x();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
