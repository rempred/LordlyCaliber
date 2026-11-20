typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    u16 flag;
    s16 s;
    s16 t;
    u8 rgba[4];
} Vtx514;
void func_00056a1c(Vtx514 *v, s16 x0, s16 y0, s16 x1, s16 y1) {
    s32 cx = 160;
    s32 cy = 120;
    s16 sw = (x1 - x0 > 0 ? x1 - x0 : x0 - x1) - 1;
    s16 sh = (y1 - y0 > 0 ? y1 - y0 : y0 - y1) - 1;
    v[9].x = cx - x1;
    v[9].y = cy - y1;
    v[9].s = 0;
    v[9].t = sh << 6;
    v[10].x = cx - x1;
    v[10].y = cy - y0;
    v[10].s = 0;
    v[10].t = 0;
    v[11].x = cx - x0;
    v[11].y = cy - y0;
    v[11].s = sw << 6;
    v[11].t = 0;
    v[12].x = cx - x0;
    v[12].y = cy - y1;
    v[12].s = sw << 6;
    v[12].t = sh << 6;
}
