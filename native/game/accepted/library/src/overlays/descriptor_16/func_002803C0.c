typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
typedef struct {
    u8 unk00[0x18];
    s32 *w18;
    s16 h1C;
    s16 h1E;
    u8 *w20;
    u8 *w24;
    s16 h28;
} Obj27;
extern Gfx *gTitleCardDisplayListCursor;
extern Obj27 *D_8022CB20;
void func_0027EC18(Obj27 *o);
void func_002803C0(void) {
    Obj27 *o = D_8022CB20;
    Gfx *g = gTitleCardDisplayListCursor;
    g->w0 = 0xFA000000;
    gTitleCardDisplayListCursor = g + 1;
    g->w1 = o->w20[0x19] | 0xFFFFFF00;
    func_0027EC18(o);
}
