/*
 * Test: Nested dereference with conditional and function call.
 * Mimics the STlib_updateMultIcon pattern from Doom's st_lib.c:
 *
 *   if (*mi->on && (mi->oldinum != *mi->inum || refresh) && (*mi->inum != -1))
 *       if (mi->oldinum != -1)
 *           x = mi->x - (short)(mi->p[mi->oldinum]->leftoffset);
 *           y = mi->y - (short)(mi->p[mi->oldinum]->topoffset);
 *           w = (short)(mi->p[mi->oldinum]->width);
 *           h = (short)(mi->p[mi->oldinum]->height);
 *           copyRect(x, y, w, h, x, y+168);  // 7 args like V_CopyRect
 *       drawPatch(mi->x, mi->y, mi->p[*mi->inum]);
 */

struct patch {
    short width;
    short height;
    short leftoffset;
    short topoffset;
    int data[4];
};

struct multicon {
    int x;
    int y;
    int oldinum;
    int *inum;
    int *on;
    struct patch **p;
};

/* 7-arg function to match V_CopyRect signature */
int g_result = 0;

void copyRect(int srcx, int srcy, int w, int h, int destx, int desty, int extra) {
    g_result = srcx + srcy + w + h + destx + desty + extra;
}

void drawPatch(int x, int y, struct patch *p) {
    g_result += x + y + p->width;
}

int updateMultIcon(struct multicon *mi, int refresh) {
    int w, h, x, y;
    if (*mi->on && (mi->oldinum != *mi->inum || refresh) && (*mi->inum != -1)) {
        if (mi->oldinum != -1) {
            x = mi->x - (int)((signed short)mi->p[mi->oldinum]->leftoffset);
            y = mi->y - (int)((signed short)mi->p[mi->oldinum]->topoffset);
            w = (int)((signed short)mi->p[mi->oldinum]->width);
            h = (int)((signed short)mi->p[mi->oldinum]->height);

            if (y < 0) return -1;

            copyRect(x, y, w, h, x, y + 168, 1);
        }
        drawPatch(mi->x, mi->y, mi->p[*mi->inum]);
        mi->oldinum = *mi->inum;
    }
    return g_result;
}

int main(void) {
    struct patch p0, p1, p2;
    struct patch *arr[3];
    struct multicon mi;
    int inum_val, on_val;

    p0.width = 10; p0.height = 20; p0.leftoffset = 3; p0.topoffset = 5;
    p1.width = 30; p1.height = 40; p1.leftoffset = 7; p1.topoffset = 9;
    p2.width = 50; p2.height = 60; p2.leftoffset = 11; p2.topoffset = 13;

    arr[0] = &p0;
    arr[1] = &p1;
    arr[2] = &p2;

    on_val = 1;
    inum_val = 1;

    mi.x = 100;
    mi.y = 200;
    mi.oldinum = 0;
    mi.inum = &inum_val;
    mi.on = &on_val;
    mi.p = arr;

    /* Call 1: oldinum=0, inum=1, refresh=1
     * x = 100 - 3 = 97
     * y = 200 - 5 = 195
     * w = 10, h = 20
     * copyRect(97, 195, 10, 20, 97, 195+168=363, 1) → g_result = 97+195+10+20+97+363+1 = 783
     * drawPatch(100, 200, p1) → g_result += 100+200+30 = 783+330 = 1113
     * oldinum → 1
     */
    int r1 = updateMultIcon(&mi, 1);
    if (r1 != 1113) return 1;

    /* Call 2: oldinum=1, inum=1, refresh=0 → condition fails (oldinum == *inum && !refresh) */
    g_result = 0;
    int r2 = updateMultIcon(&mi, 0);
    if (r2 != 0) return 2;

    /* Call 3: change inum to 2, oldinum=1, refresh=0
     * x = 100 - 7 = 93
     * y = 200 - 9 = 191
     * w = 30, h = 40
     * copyRect(93, 191, 30, 40, 93, 191+168=359, 1) → g_result = 93+191+30+40+93+359+1 = 807
     * drawPatch(100, 200, p2) → g_result += 100+200+50 = 807+350 = 1157
     * oldinum → 2
     */
    g_result = 0;
    inum_val = 2;
    int r3 = updateMultIcon(&mi, 0);
    if (r3 != 1157) return 3;

    return 0x55; // expected=0x55
}

void interrupt(void) {}
