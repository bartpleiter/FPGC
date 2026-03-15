int main() {
    int i = 0;
    int result = 0;
    
    // Empty loop body
    for (i = 0; i < 5; i++);
    
    result = i + 2;
    return result; // expected=0x07
}

void interrupt()
{

}
