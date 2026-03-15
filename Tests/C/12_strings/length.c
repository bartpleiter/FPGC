int strlen_manual(char *s) {
    int len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

int main(void) {
    return strlen_manual("abcdefg"); // expected=0x07
}

void interrupt(void) {}
