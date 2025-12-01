int main() {
    int i = 0;
    int sum = 0;
    
    do {
        sum = sum + i;
        i++;
    } while (i < 5);
    
    return sum - 3; // expected=0x07
}

void interrupt()
{

}
