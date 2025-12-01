struct Rectangle {
    int width;
    int height;
};

int area(struct Rectangle r)
{
    return r.width * r.height;
}

int main() {
    struct Rectangle rect;
    rect.width = 7;
    rect.height = 1;
    return area(rect); // expected=0x07
}

void interrupt()
{

}
