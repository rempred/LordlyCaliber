typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *gTitleCardDisplayListCursor;
typedef struct {
    s32 m[16];
} Mtx40;
extern Mtx40 *D_801969B0;
extern Gfx D_80225620[];
void guTranslateF(f32 mf[4][4], f32 x, f32 y, f32 z);
void guMtxF2L(f32 mf[4][4], void *m);
s32 func_00275850(u8 *o, s32 k);
void func_00275AF8(u8 *o, s32 a);
void func_00275C98(u8 *o) {
    f32 m[4][4];
    Gfx *g;
    guTranslateF(m, 0.0f, 0.0f, 0.0f);
    guMtxF2L(m, D_801969B0);
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDA380000;
    g->w1 = (u32)D_801969B0++;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDE000000;
    g->w1 = (u32)D_80225620;
    func_00275AF8(o, func_00275850(o, 0));
    func_00275AF8(o, func_00275850(o, 1));
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xD8380002;
    g->w1 = 0x40;
}
