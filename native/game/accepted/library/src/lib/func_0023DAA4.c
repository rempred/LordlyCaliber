typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
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
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *gTitleCardDisplayListCursor;
extern Gfx D_801F0028[];
void D_801B5CAC(s32 a, s32 b, s32 c);
void func_0023DAA4(void) {
    Ctx23 *s = D_801EFE48;
    Gfx *g;
    if (s->flags & 1) {
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xDE000000;
        g->w1 = (u32)D_801F0028;
        D_801B5CAC((*(s32 *)((u8 *)(s) + (0x1A0))), 0, 0);
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xE7000000;
        g->w1 = 0;
    }
}
