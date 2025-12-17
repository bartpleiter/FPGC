/*
 * Test: BRFS string operations and path parsing
 * Tests the helper functions that can run without SPI Flash hardware
 */

#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_BRFS
#include "libs/kernel/kernel.h"

int main()
{
    unsigned int compressed[4];
    char decompressed[17];
    
    /*========================================================================
     * String Compression Tests
     *========================================================================*/
    
    /* Test 1: Simple 4-char filename compression */
    brfs_compress_string(compressed, "test");
    /* 't' = 0x74, 'e' = 0x65, 's' = 0x73, 't' = 0x74 */
    if ((compressed[0] >> 24) != 't') return 1;
    if (((compressed[0] >> 16) & 0xFF) != 'e') return 2;
    if (((compressed[0] >> 8) & 0xFF) != 's') return 3;
    if ((compressed[0] & 0xFF) != 't') return 4;
    
    /* Test 2: Decompression round-trip */
    brfs_decompress_string(decompressed, compressed, 4);
    if (decompressed[0] != 't') return 5;
    if (decompressed[1] != 'e') return 6;
    if (decompressed[2] != 's') return 7;
    if (decompressed[3] != 't') return 8;
    if (decompressed[4] != '\0') return 9;
    
    /* Test 3: Single character filename */
    brfs_compress_string(compressed, "a");
    brfs_decompress_string(decompressed, compressed, 4);
    if (decompressed[0] != 'a') return 10;
    if (decompressed[1] != '\0') return 11;
    
    /* Test 4: 8-char filename (spans 2 words) */
    brfs_compress_string(compressed, "testfile");
    brfs_decompress_string(decompressed, compressed, 4);
    if (strcmp(decompressed, "testfile") != 0) return 12;
    
    /* Test 5: Maximum 16-char filename (spans all 4 words) */
    brfs_compress_string(compressed, "1234567890ABCDEF");
    brfs_decompress_string(decompressed, compressed, 4);
    if (decompressed[0] != '1') return 13;
    if (decompressed[15] != 'F') return 14;
    
    /* Test 6: Filename with extension */
    brfs_compress_string(compressed, "data.bin");
    brfs_decompress_string(decompressed, compressed, 4);
    if (strcmp(decompressed, "data.bin") != 0) return 15;
    
    /* All tests passed */
    return 42; // expected=0x2A
}

void interrupt()
{
}
