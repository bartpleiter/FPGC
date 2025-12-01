char g_str[] = "Global";

int main() {
    return g_str[0] - 64; // expected=0x07
}

void interrupt()
{

}
