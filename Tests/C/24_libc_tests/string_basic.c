/*
 * Test: String library functions
 * Tests memcpy, memset, strlen, strcmp, strcpy
 */

#define COMMON_STRING
#include "libs/common/common.h"

int main() {
    char buf1[10];
    char buf2[10];
    int result = 0;
    
    /* Test strlen */
    if (strlen("hello") != 5) return 1;
    if (strlen("") != 0) return 2;
    
    /* Test strcpy */
    strcpy(buf1, "test");
    if (buf1[0] != 't') return 3;
    if (buf1[4] != '\0') return 4;
    
    /* Test strcmp */
    strcpy(buf1, "abc");
    strcpy(buf2, "abc");
    if (strcmp(buf1, buf2) != 0) return 5;
    
    strcpy(buf2, "abd");
    if (strcmp(buf1, buf2) >= 0) return 6;
    
    /* Test memset */
    memset(buf1, 'x', 4);
    buf1[4] = '\0';
    if (buf1[0] != 'x') return 77;
    if (buf1[3] != 'x') return 8;
    
    /* Test memcpy */
    strcpy(buf1, "hello");
    memcpy(buf2, buf1, 6);
    if (strcmp(buf1, buf2) != 0) return 9;
    
    /* Test strncmp */
    if (strncmp("hello", "help", 3) != 0) return 10;
    if (strncmp("hello", "help", 4) >= 0) return 11;
    
    /* All tests passed */
    return 7; // expected=0x07
}

void interrupt()
{
}
