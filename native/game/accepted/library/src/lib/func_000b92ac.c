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
void func_000b92ac(s16 i);
void func_000b92ac(s16 i) {
    AnimSlot *e = &D_80196AF8->slots[i];
    if (e->cur[0] == e->start[0] && e->cur[1] == e->start[1] && e->cur[2] == e->start[2] && e->cur[3] == e->start[3]) {
        s32 d0;
        s32 d1;
        s32 top;
        s32 bot;
        d0 = e->cur[0] - 160;
        if (d0 < 0) {
            d0 = -d0;
        }
        d1 = e->cur[2] - 160;
        if (d1 < 0) {
            d1 = -d1;
        }
        if (d1 < d0) {
            e->v1C = e->cur[0];
            e->v24 = e->cur[0] + 8;
        } else {
            e->v1C = e->cur[2] - 8;
            e->v24 = e->cur[2];
        }
        top = e->cur[1];
        bot = e->cur[3];
        d0 = top - 120;
        if (d0 < 0) {
            d0 = -d0;
        }
        d1 = bot - 120;
        if (d1 < 0) {
            d1 = -d1;
        }
        if (d1 < d0) {
            e->v20 = e->cur[1];
            e->v28 = e->cur[1] + 8;
        } else {
            e->v20 = e->cur[3] - 8;
            e->v28 = e->cur[3];
        }
    } else {
        e->v1C = e->start[0];
        e->v20 = e->start[1];
        e->v24 = e->start[2];
        e->v28 = e->start[3];
    }
    e->type = 2;
    e->mode = 0x8000;
}
