int main() {
    int a = 5;
    int *p = &a;
    
    *p = 10;  // modify through pointer
    
    return a - 3; // expected=0x07
}

void interrupt()
{

}
