int main() {
    int i = 0;
    int count = 0;
    
    while (i < 10)
    {
        i++;
        if (i == 3)
            continue;
        if (i == 8)
            break;
        count++;
    }
    
    return count + 1; // expected=0x07
}

void interrupt()
{

}
