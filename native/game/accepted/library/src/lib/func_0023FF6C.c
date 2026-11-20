typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;
typedef struct {
    u8 unk00[0x60];
    u16 flags;
    u8 unk62[4];
    s16 id;
} Ent23;
typedef struct {
    Ent23 ent[4];
    u8 unk1A0[0x1D0];
    s16 x0;
    s16 x1;
    s16 y0;
    s16 y1;
    u8 unk378[0x2D8];
    u16 flags;
    s16 h652;
    u8 unk654[2];
    s16 fade;
} Ctx23;
extern Ctx23 *D_801EFE48;
void func_00022e90(f32 mf[4][4], f32 r, f32 p, f32 h);
void guMtxCatF(f32 m[4][4], f32 n[4][4], f32 r[4][4]);
typedef struct {
    u8 unk00[4];
    f32 m[4][4];
    s16 h44;
    u8 unk46[0xA];
    s16 h50;
    u8 unk52[2];
} Part23;
void func_0023FF6C(void) {
    f32 m[4][4];
    s32 i;
    Part23 *e;
    f32 (*mtx)[4];
    if (!((*(u16 *)((u8 *)(D_801EFE48) + (0x650))) & 2)) {
        return;
    }
    for (i = 0; i < 7; i++) {
        e = (Part23 *)((u8 *)D_801EFE48 + 0x378) + i;
        mtx = e->m;
        if (e->h44 < e->h50) {
            e->h44 += 2;
            if (e->h44 < e->h50) {
                goto rot;
            }
        }
        e->h44 = e->h50;
    rot:
        func_00022e90(m, 5.0f, 0.0f, 0.0f);
        guMtxCatF(m, mtx, mtx);
    }
}
