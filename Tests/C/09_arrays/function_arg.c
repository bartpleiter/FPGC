void fill_array(int *arr, int size)
{
    int i;
    for (i = 0; i < size; i++)
    {
        arr[i] = i * 2;
    }
}

int main() {
    int buf[5];
    fill_array(buf, 5);
    return buf[3] + 1; // expected=0x07
}

void interrupt()
{

}
