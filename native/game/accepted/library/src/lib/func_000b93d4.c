typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    s32 f00;
    s32 f04;
    s32 f08;
    s32 cur[4];
    s32 v1C;
    s32 v20;
    s32 v24;
    s32 v28;
    s32 start[4];
    u8 unk03C[0xAA];
    u8 flags;
    u8 unk0E7;
    u16 mode;
    u16 type;
    u16 unk0EC;
    u16 id;
    s32 unk0F0;
    void *data;
    u8 unk0F8[0x10];
} AnimSlot;
typedef struct {
    u8 unk0000[0x650];
    AnimSlot slots[10];
    u16 count;
} AnimState;
extern AnimState *D_80196AF8;
s32 func_000b94b0(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h);
s32 func_000b93d4(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h, s16 x2, s16 y2, s16 w2, s16 h2) {
    s32 i = func_000b94b0(kind, b, fn, x, y, w, h);
    AnimSlot *e = &D_80196AF8->slots[i];
    e->type = 1;
    e->v1C = x2;
    e->v20 = y2;
    e->v24 = w2;
    e->v28 = h2;
    e->mode = 0x8000;
    return i;
}
