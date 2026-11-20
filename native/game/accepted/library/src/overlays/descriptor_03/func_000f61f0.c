typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
typedef struct {
    u8 unk000[0x104];
    s32 w104;
    u8 unk108[4];
    f32 f10C;
    u8 unk110[0x3E];
    u8 r;
    u8 g;
    u8 b;
    u8 unk151[0xB];
    f32 f15C;
} Scene0E;
extern Gfx *gTitleCardDisplayListCursor;
extern Scene0E *D_801ABA58;
void func_000f61f0(void) {
    Gfx *g = gTitleCardDisplayListCursor;
    Scene0E *s = D_801ABA58;
    gTitleCardDisplayListCursor = g + 1;
    g->w0 = 0xE7000000;
    gTitleCardDisplayListCursor = g + 2;
    g->w1 = 0;
    g[1].w0 = 0xFA000000;
    g[1].w1 = (s->r << 24) | (s->g << 16) | (s->b << 8) | 0xFF;
}
