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
s32 func_0000735C(s32 a, void *fn, s32 b, s32 c, s32 d, s32 e, s32 f);
extern s32 *D_80214E60;
extern u8 D_80214EB8;
extern u8 D_80214EB9;
void func_00155380();
void func_00155290(u8 mode, s32 *p, u8 b, u8 c) {
    s32 i;
    D_80214E60 = p;
    if (mode) {
        D_80214EB9 = *p | 0x80;
    } else {
        D_80214EB8 = b;
        if (b < 8) {
            D_80214EB8 = b | 0x80;
        } else {
            D_80214EB8 = 0x80;
        }
        D_80214EB9 = D_80214EB8;
    }
    i = func_0000735C(0xC000, func_00155380, 0x58, 0x78, 0x62, 0x82, 0xC8);
    g_state_slot_records[i].unk88 = b;
    g_state_slot_records[i].unk89 = c;
    g_state_slot_records[i].unk8A = mode;
}
