typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 flags;
    u8 flags2;
    u8 flags3;
    u8 unk04[2];
    s16 x;
    s16 y;
    s16 x2;
    s16 y2;
    u16 unk0E;
    void *owner;
    u8 unk14[0x14];
    s16 unk28;
    s16 unk2A;
    u8 unk2C[0xC];
    void *slotsA[10];
    void *slotsB[10];
    u8 unk88;
    u8 unk89;
    u8 unk8A;
    u8 unk8B;
    u8 unk8C;
    u8 unk8D;
    u8 unk8E;
    u8 unk8F;
    u8 unk90[4];
    u8 unk94;
    u8 unk95;
    u8 unk96;
    u8 unk97;
    u8 unk98[0xA];
    s16 parent;
    u8 unkA4[4];
} Scene;
extern Scene g_state_slot_records[];
extern u8 D_80213780;
extern u8 D_80213781;
extern signed char D_80214D10[];
s32 func_0000735C(s32 a, void *fn, s32 b, s32 c, s32 d, s32 e, s32 f);
void func_0015709C();
void func_0015DF10(s32 a);
void func_00023940(const signed char *fmt, ...);
s32 func_00156E90(u8 a) {
    u8 *f = &D_80213780;
    s32 i;
    s32 r;
    if (*f != 0) {
        *f = 0;
        D_80213781 = a;
        i = func_0000735C(0xC000, func_0015709C, 0x19, 0x28, 0x23, 0x32, 0xC8);
        g_state_slot_records[i].unk88 = 0xFF;
        func_0015DF10(6);
        r = 1;
    } else {
        func_00023940(D_80214D10);
        r = 0;
    }
    return r;
}
