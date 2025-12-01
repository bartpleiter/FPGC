int counter = 0;

void increment()
{
    counter = counter + 1;
}

int main() {
    increment();
    increment();
    increment();
    increment();
    increment();
    increment();
    increment();
    return counter; // expected=0x07
}

void interrupt()
{

}
