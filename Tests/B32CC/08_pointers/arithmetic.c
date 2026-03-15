int main() {
    int arr[5];
    int *p = arr;
    
    *p = 1;
    *(p + 1) = 2;
    *(p + 2) = 3;
    
    return *p + *(p + 1) + *(p + 2) + 1; // expected=0x07
}

void interrupt()
{

}
