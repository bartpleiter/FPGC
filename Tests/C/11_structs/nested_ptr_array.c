/*
 * Test nested pointer dereference through struct member array:
 *   outer->ptr_array[index]->field
 *
 * This exercises the pattern used in Doom's STlib_updateMultIcon:
 *   mi->p[mi->oldinum]->leftoffset
 *
 * Chain: load struct ptr → load array ptr → index → load element ptr → load field
 */

struct inner {
    short width;
    short height;
    short leftoffset;
    short topoffset;
};

struct outer {
    int x;
    int y;
    int oldindex;
    struct inner **items;  /* array of pointers to inner structs */
};

int read_nested(struct outer *o) {
    int x = o->x - (int)o->items[o->oldindex]->leftoffset;
    int y = o->y - (int)o->items[o->oldindex]->topoffset;
    int w = (int)o->items[o->oldindex]->width;
    int h = (int)o->items[o->oldindex]->height;
    return x + y + w + h;
}

int main(void) {
    struct inner a, b, c;
    struct inner *arr[3];
    struct outer o;

    a.width = 10; a.height = 20; a.leftoffset = 3; a.topoffset = 5;
    b.width = 30; b.height = 40; b.leftoffset = 7; b.topoffset = 9;
    c.width = 50; c.height = 60; c.leftoffset = 11; c.topoffset = 13;

    arr[0] = &a;
    arr[1] = &b;
    arr[2] = &c;

    o.x = 100;
    o.y = 200;
    o.items = arr;

    /* Test index 0: x=100-3=97, y=200-5=195, w=10, h=20 → 322 */
    o.oldindex = 0;
    int r0 = read_nested(&o);
    if (r0 != 322) return 1;

    /* Test index 1: x=100-7=93, y=200-9=191, w=30, h=40 → 354 */
    o.oldindex = 1;
    int r1 = read_nested(&o);
    if (r1 != 354) return 2;

    /* Test index 2: x=100-11=89, y=200-13=187, w=50, h=60 → 386 */
    o.oldindex = 2;
    int r2 = read_nested(&o);
    if (r2 != 386) return 3;

    return 42; // expected=0x2A
}

void interrupt(void) {}
