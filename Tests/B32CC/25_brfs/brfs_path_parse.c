/*
 * Test: BRFS path parsing
 * Tests the path parsing helper function
 */

#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_BRFS
#include "libs/kernel/kernel.h"

int main()
{
    char dir_path[32];
    char filename[17];
    int result;
    
    /* Test 1: Simple file in root */
    result = brfs_parse_path("/test.txt", dir_path, filename, 32);
    if (result != BRFS_OK) return 1;
    if (strcmp(dir_path, "/") != 0) return 2;
    if (strcmp(filename, "test.txt") != 0) return 3;
    
    /* Test 2: File without leading slash (treated as root) */
    result = brfs_parse_path("myfile.c", dir_path, filename, 32);
    if (result != BRFS_OK) return 4;
    if (strcmp(dir_path, "/") != 0) return 5;
    if (strcmp(filename, "myfile.c") != 0) return 6;
    
    /* Test 3: File in subdirectory */
    result = brfs_parse_path("/sub/file.txt", dir_path, filename, 32);
    if (result != BRFS_OK) return 7;
    if (strcmp(dir_path, "/sub") != 0) return 8;
    if (strcmp(filename, "file.txt") != 0) return 9;
    
    /* Test 4: Nested directory path */
    result = brfs_parse_path("/a/b/test.dat", dir_path, filename, 32);
    if (result != BRFS_OK) return 10;
    if (strcmp(dir_path, "/a/b") != 0) return 11;
    if (strcmp(filename, "test.dat") != 0) return 12;
    
    /* All tests passed */
    return 42; // expected=0x2A
}

void interrupt()
{
}
