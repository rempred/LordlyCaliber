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
extern u16 D_800C4C24;
extern u16 D_800E7A0E;
extern u8 D_80216F18[];
extern FadeCtx *D_80216BE0;
void func_000225d0(void *m, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);
void func_000210c0(void *m, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp);
void func_001A37BC(void);
void func_001A37BC(void) {
    FadeCtx *ctx = D_80216BE0;
    ctx->cs.cam->eye[0] = 160.0f;
    ctx->cs.cam->eye[1] = 120.0f;
    ctx->cs.cam->eye[2] = 60.0f;
    ctx->cs.cam->at[0] = 160.0f;
    ctx->cs.cam->at[1] = 120.0f;
    ctx->cs.cam->at[2] = 0.0f;
    ctx->cs.cam->up[0] = 0.0f;
    ctx->cs.cam->up[1] = 1.0f;
    ctx->cs.cam->up[2] = 0.0f;
    D_800E7A0E = gCardMatrixCount;
    func_000225d0(D_800E9BE0->mtx[gCardMatrixCount], -160.0f, 160.0f, -120.0f, 120.0f, 1.0f, 2000.0f, 1.0f);
    {
        Gfx *_g = gTitleCardDisplayListCursor++;
        _g->w0 = 0xDA380007;
        _g->w1 = (u32)D_800E9BE0->mtx[gCardMatrixCount] - 0x80000000;
    }
    D_800C4C24 = ++gCardMatrixCount;
    func_000210c0(D_800E9BE0->mtx[D_800C4C24], ctx->cs.cam->eye[0], ctx->cs.cam->eye[1], ctx->cs.cam->eye[2],
             ctx->cs.cam->at[0], ctx->cs.cam->at[1], ctx->cs.cam->at[2], ctx->cs.cam->up[0], ctx->cs.cam->up[1],
             ctx->cs.cam->up[2]);
    {
        Gfx *_g = gTitleCardDisplayListCursor++;
        _g->w0 = 0xDA380005;
        _g->w1 = (u32)D_800E9BE0->mtx[gCardMatrixCount++] - 0x80000000;
    }
    {
        Gfx *_g = gTitleCardDisplayListCursor++;
        _g->w0 = 0xDE000000;
        _g->w1 = (u32)D_80216F18;
    }
}
