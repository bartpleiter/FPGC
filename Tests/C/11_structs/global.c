struct Data {
    int a;
    int b;
    int c;
};

struct Data g_data;

int main() {
    g_data.a = 1;
    g_data.b = 2;
    g_data.c = 4;
    return g_data.a + g_data.b + g_data.c; // expected=0x07
}

void interrupt()
{

}
