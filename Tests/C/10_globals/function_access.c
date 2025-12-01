int glob = 5;

int add_to_glob(int x)
{
    return glob + x;
}

int main() {
    return add_to_glob(2); // expected=0x07
}

void interrupt()
{

}
