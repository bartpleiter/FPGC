int add(int a, int b);  // forward declaration

int main() {
    return add(3, 4); // expected=0x07
}

int add(int a, int b)
{
    return a + b;
}

void interrupt()
{

}
