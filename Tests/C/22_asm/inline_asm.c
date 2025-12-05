// Test inline assembly with B32CC
// This test edits the return value from the stack

int main() {
    int a = 5;
    asm(
        "load32 7 r1      ; set r1 to 7"
        "write -1 r14 r1  ; write 7 to stack at place of int a"
    );
    return a; // expected=0x07
}

void interrupt()
{

}
