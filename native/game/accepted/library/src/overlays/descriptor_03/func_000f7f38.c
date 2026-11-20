typedef signed short s16;
typedef signed int s32;
s32 func_0000745C(s32 a, void *b, s32 c, s32 d, s32 e, s32 f, s32 g);
void func_000f8004(s16 id);
typedef unsigned char u8;
typedef unsigned short u16;
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
static void func_000F7F74(u8 i, u8 a, u8 b, u8 c, u8 d, u8 e, u8 f, u8 g, u8 h);
void func_000f7f38(void) {
    func_0000745C(0xC000, func_000f8004, 0, 0, 0, 0, 0xF000);
}
static void func_000F7F74(u8 i, u8 a, u8 b, u8 c, u8 d, u8 e, u8 f, u8 g, u8 h) {
    g_state_slot_records[i].unk88 = a;
    g_state_slot_records[i].unk89 = b;
    g_state_slot_records[i].unk8A = c;
    g_state_slot_records[i].unk8B = d;
    g_state_slot_records[i].unk8C = e;
    g_state_slot_records[i].unk8D = f;
    g_state_slot_records[i].unk8E = g;
    g_state_slot_records[i].unk8F = h;
}
