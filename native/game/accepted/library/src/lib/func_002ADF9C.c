typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern s32 D_801CEAB0;
extern u8 *D_801D06F4;
extern Gfx *gTitleCardDisplayListCursor;
void func_002ADF9C(u8 *obj) {
    f32 y = (*(f32 *)((u8 *)(obj) + (0x11C))) + 152.0f + (*(f32 *)((u8 *)(obj) + (0x124))) + 38.0f;
    u8 *col = &D_801D06F4[D_801CEAB0 * 6];
    f32 t;
    f32 r;
    f32 g;
    f32 b;
    Gfx *gfx;
    if (y < 0.0f) {
        y = 0.0f;
    } else if (380.0f < y) {
        y = 380.0f;
    }
    t = 380.0f - y;
    r = t * (col[3] - (f32)col[0]) / 380.0f + col[0];
    g = t * (col[4] - (f32)col[1]) / 380.0f + col[1];
    b = t * (col[5] - (f32)col[2]) / 380.0f + col[2];
    gfx = gTitleCardDisplayListCursor++;
    gfx->w0 = 0xFB000000;
    gfx->w1 = ((u32)r << 24) | (((u32)g & 0xFF) << 16) | (((u32)b & 0xFF) << 8) | 1;
}
