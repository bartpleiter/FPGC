int main() {
    char buf[10];
    
    buf[0] = 30;
    buf[1] = 31;
    buf[2] = 32;
    
    char c = buf[0] + buf[1] + buf[2];
    return c - 86; // expected=0x07
}

void interrupt()
{

}
