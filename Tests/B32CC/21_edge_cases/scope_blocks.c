int main() {
    int a = 7;
    {
        int b = 5;  // inner scope variable
        a = a + b - 5;
    }
    // b is out of scope here
    return a; // expected=0x07
}

void interrupt()
{

}
