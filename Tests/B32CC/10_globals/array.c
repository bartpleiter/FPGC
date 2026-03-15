int g_arr[5];

int main() {
    g_arr[0] = 1;
    g_arr[1] = 2;
    g_arr[2] = 3;
    g_arr[3] = 4;
    g_arr[4] = 5;
    
    return g_arr[2] + g_arr[3]; // expected=0x07
}

void interrupt()
{

}
