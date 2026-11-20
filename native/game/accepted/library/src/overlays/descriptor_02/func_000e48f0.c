typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *gTitleCardDisplayListCursor;
extern s32 D_8019E180;
extern u8 D_8018F480;
extern u8 D_80186BD0[];
void func_000e48f0(void) {
    Gfx *g;
    D_8019E180 = 8;
    D_8018F480 = 0;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDE000000;
    g->w1 = (u32)D_80186BD0;
}
