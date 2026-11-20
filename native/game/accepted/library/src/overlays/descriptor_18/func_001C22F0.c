typedef signed short s16;
typedef signed int s32;
typedef struct {
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
} Rect1BB;
s32 func_001C22F0(Rect1BB *a, Rect1BB *b, Rect1BB *out) {
    s32 n = 0;
    if (a->x0 < b->x0) {
        out[0].x0 = a->x0;
        out[0].x1 = b->x0;
        out[0].y0 = a->y0;
        out[0].y1 = a->y1;
        n = 1;
    }
    if (b->x1 < a->x1) {
        out[n].x0 = b->x1;
        out[n].x1 = a->x1;
        out[n].y0 = a->y0;
        out[n].y1 = a->y1;
        n++;
    }
    if (b->y0 > a->y0) {
        out[n].x0 = b->x0;
        out[n].x1 = b->x1;
        out[n].y0 = a->y0;
        out[n].y1 = b->y0;
        n++;
    }
    if (b->y1 < a->y1) {
        out[n].x0 = b->x0;
        out[n].x1 = b->x1;
        out[n].y0 = b->y1;
        out[n].y1 = a->y1;
        n++;
    }
    return n;
}
