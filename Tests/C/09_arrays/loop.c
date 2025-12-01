int main() {
    int arr[5];
    int i;
    int sum = 0;
    
    for (i = 0; i < 5; i++)
    {
        arr[i] = i + 1;
    }
    
    for (i = 0; i < 5; i++)
    {
        sum = sum + arr[i];
    }
    
    return sum - 8; // expected=0x07
}

void interrupt()
{

}
