int strlen_custom(char *str)
{
    int len = 0;
    while (*str != 0)
    {
        len++;
        str++;
    }
    return len;
}

int main() {
    char *s = "Hello!!";
    return strlen_custom(s); // expected=0x07
}

void interrupt()
{

}
