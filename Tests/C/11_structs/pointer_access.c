struct Rect {
    int x;
    int y;
    int w;
    int h;
};

int area(struct Rect *r) {
    return r->w * r->h;
}

int main(void) {
    struct Rect r;
    r.x = 0;
    r.y = 0;
    r.w = 7;
    r.h = 1;
    return area(&r); // expected=0x07
}

void interrupt(void) {}
