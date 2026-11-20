typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {
    f32 persp[5];
    f32 eye[3];
    f32 at[3];
    f32 up[3];
    s32 rest[8];
} Camera;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
typedef struct {
    void *p0;
    void *p1;
    u8 (*mtx)[0x40];
} FbSet;
typedef struct {
    Camera *cam;
    Camera *saved;
} CamSave;
typedef struct {
    void *image;
} TexRef;
typedef struct {
    s16 unk00;
    CamSave cs;
    TexRef tex;
    s16 unk10;
    s16 unk12;
    s16 unk14;
    s16 unk16;
    s16 unk18;
    f32 scaleX;
    f32 scaleY;
    u8 unk24[0xC];
    void *unk30;
    u8 unk34[6];
    s16 alpha;
    f32 unk3C;
    f32 unk40;
    u8 unk44[0x10];
} FadeCtx;
extern FbSet *D_800E9BE0;
extern Gfx *gTitleCardDisplayListCursor;
extern u16 gCardMatrixCount;
extern u8 D_80216C70[];
extern u8 D_80216CF0[];
extern u8 D_80216D70[];
extern FadeCtx *D_80216BE0;
extern u16 D_80216F70;
void func_00028F60(f32 mf[4][4], f32 x, f32 y, f32 z);
void guTranslateF(f32 mf[4][4], f32 x, f32 y, f32 z);
void guMtxIdent(void *m);
void guMtxCatF(f32 a[4][4], f32 b[4][4], f32 out[4][4]);
void guMtxF2L(f32 mf[4][4], void *m);
void func_001A3D2C(void);
void func_001A3D2C(void) {
    FadeCtx *ctx = D_80216BE0;
    f32 mf[4][4];
    f32 scale[4][4];
    if (!(D_80216F70 & 2)) {
        return;
    }
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xDE000000); _g->w1 = ((u32)D_80216C70); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xFB000000); _g->w1 = (0xFF); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xFD700000); _g->w1 = ((u32)ctx->unk30); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xF5700000); _g->w1 = (0x07014050); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xE6000000); _g->w1 = (0); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xF3000000); _g->w1 = (0x071FF200); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xE7000000); _g->w1 = (0); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xF5680800); _g->w1 = (0x00014050); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xF2000000); _g->w1 = (0x0007C07C); };
    guMtxIdent(D_800E9BE0->mtx[gCardMatrixCount]);
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xDA380003); _g->w1 = ((u32)D_800E9BE0->mtx[gCardMatrixCount++] - 0x80000000); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0x01004008); _g->w1 = ((u32)D_80216CF0); };
    func_00028F60(scale, ctx->unk3C, ctx->unk40, 0.0f);
    guTranslateF(mf, 159.0f, 119.0f, 0.0f);
    guMtxCatF(scale, mf, mf);
    guMtxF2L(mf, D_800E9BE0->mtx[gCardMatrixCount]);
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xDA380003); _g->w1 = ((u32)D_800E9BE0->mtx[gCardMatrixCount++] - 0x80000000); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0x01004010); _g->w1 = ((u32)(D_80216CF0 + 0x40)); };
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xDE000000); _g->w1 = ((u32)D_80216D70); };
}
