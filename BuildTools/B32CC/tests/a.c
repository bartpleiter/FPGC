// Temporary test file. Eventually should make proper tests under Software/BareMetalBCC folder
void g() {
    int q = 5;
}

void f() {
    int z = 5;
    int x = 4;
    g();
    int y = z + x;

}

int main() {
    int a = 3;
    int b = 4;
    f();
    int c = a + b;
    return c; //7
}
