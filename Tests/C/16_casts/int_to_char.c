int main() {
    int a = (char)300;  // truncate to 8-bit: 300 & 0xFF = 44
    return a - 37; // expected=0x07
}

void interrupt()
{

}
