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
extern s32 D_80190F20;
extern u8 *D_801977CC;
extern u8 *D_800C4BB8;
extern u16 g_state_slot_status;
void func_00019fc0(void);
void bcopy(const void *src, void *dst, s32 n);
static void func_000499C4(void);
void func_00049918(void) {
    func_00019fc0();
    if (g_state_slot_records[D_80190F20].unk8B & 2) {
        bcopy(D_800C4BB8, D_801977CC, 0x25800);
        g_state_slot_records[D_80190F20].flags2 |= 2;
        g_state_slot_status = 0xFFFE;
    }
}
static void func_000499C4(void) {
}
