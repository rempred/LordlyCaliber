typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    u16 flag;
    s16 s;
    s16 t;
    u8 rgba[4];
} VtxB8;
void func_000b9768(VtxB8 *v, s16 x0, s16 y0, s16 x1, s16 y1) {
    s16 sw = (x1 - x0 > 0 ? x1 - x0 : x0 - x1) - 1;
    s16 sh = (y1 - y0 > 0 ? y1 - y0 : y0 - y1) - 1;
    v[6].x = 160 - x1;
    v[6].y = 120 - y1;
    v[6].s = 0;
    v[6].t = sh << 6;
    v[7].x = 160 - x1;
    v[7].y = 120 - y0;
    v[7].s = 0;
    v[7].t = 0;
    v[8].x = 160 - x0;
    v[8].y = 120 - y0;
    v[8].s = sw << 6;
    v[8].t = 0;
    v[9].x = 160 - x0;
    v[9].y = 120 - y1;
    v[9].s = sw << 6;
    v[9].t = sh << 6;
}
