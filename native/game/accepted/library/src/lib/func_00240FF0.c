typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
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
void func_00240FF0(void) {
    Ctx23 *s = D_801EFE48;
    s32 i;
    if ((*(void * *)((u8 *)(s) + (0x244))) != ((void *)0)) {
        func_000016C4((*(void * *)((u8 *)(s) + (0x244))));
        (*(void * *)((u8 *)(s) + (0x244))) = ((void *)0);
        (*(s32 *)((u8 *)(s) + (0x248))) = 0;
    }
    D_801EFE48->flags &= 3;
    for (i = 0; i < 2; i++) {
        u8 *e = (u8 *)D_801EFE48 + 0x5D4 + i * 0x1C;
        if ((*(void * *)((u8 *)(e) + (4))) != ((void *)0)) {
            func_000016C4((*(void * *)((u8 *)(e) + (4))));
            (*(void * *)((u8 *)(e) + (4))) = ((void *)0);
        }
    }
    D_801EFE48->flags &= ~2;
}
