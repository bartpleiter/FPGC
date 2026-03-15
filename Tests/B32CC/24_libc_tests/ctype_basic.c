/*
 * Test: ctype character classification functions
 */

#define COMMON_CTYPE
#include "libs/common/common.h"

int main() {
    /* Test isdigit */
    if (!isdigit('0')) return 1;
    if (!isdigit('9')) return 2;
    if (isdigit('a')) return 3;
    
    /* Test isalpha */
    if (!isalpha('a')) return 4;
    if (!isalpha('Z')) return 5;
    if (isalpha('5')) return 6;
    
    /* Test isalnum */
    if (!isalnum('a')) return 77;
    if (!isalnum('5')) return 8;
    if (isalnum('@')) return 9;
    
    /* Test isspace */
    if (!isspace(' ')) return 10;
    if (!isspace('\t')) return 11;
    if (!isspace('\n')) return 12;
    if (isspace('a')) return 13;
    
    /* Test isupper/islower */
    if (!isupper('A')) return 14;
    if (isupper('a')) return 15;
    if (!islower('z')) return 16;
    if (islower('Z')) return 17;
    
    /* Test toupper/tolower */
    if (toupper('a') != 'A') return 18;
    if (tolower('A') != 'a') return 19;
    if (toupper('5') != '5') return 20;
    
    /* Test isxdigit */
    if (!isxdigit('0')) return 21;
    if (!isxdigit('f')) return 22;
    if (!isxdigit('F')) return 23;
    if (isxdigit('g')) return 24;
    
    /* All tests passed */
    return 7; // expected=0x07
}

void interrupt()
{
}
