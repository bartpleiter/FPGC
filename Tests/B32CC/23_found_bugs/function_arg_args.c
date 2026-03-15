// Test a bug where using a function as an argument causes its arguments to be misread from stack

#define COMMON_STRING
#include "libs/common/common.h"

int test_strlen(int a, char* b, int c)
{
    int x = strlen(b);
    return x + c;
}

int main() {
    char* test_data_0 = "Hello";
    test_strlen(0, test_data_0, strlen(test_data_0)); // Replacing strlen with 5 avoids the bug!
    return 10; // expected=0x0A
}

void interrupt()
{

}
