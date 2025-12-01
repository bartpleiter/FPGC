int main() {
    int a = 3;
    int *p = &a;
    
    return *p + 4; // expected=0x07
}

void interrupt()
{

}
