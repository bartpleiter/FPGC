int main() {
    int i;
    int j;
    int count = 0;
    
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            count++;
        }
    }
    
    return count - 2; // expected=0x07
}

void interrupt()
{

}
