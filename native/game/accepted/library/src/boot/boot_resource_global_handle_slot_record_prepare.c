extern void *D_800AF0B0;
void *D_80173B60(void *p);
typedef unsigned char u8;
typedef signed short s16;
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
typedef signed int s32;
extern u16 D_800E810E;
extern s32 D_800C4C20;
extern inline u16 SceneFindFree(void) {
    u16 i;
    for (i = 0; i < 6; i++) {
        if (!(g_state_slot_records[i].flags & 0x8000)) {
            return i;
        }
    }
    for (;;);
}
extern inline u16 SceneFindAfterLast(void) {
    s16 i;
    for (i = 4; i >= 0; i--) {
        if (g_state_slot_records[i].flags & 0x8000) {
            return i + 1;
        }
    }
    for (;;);
}
static u16 func_00007258(void);
static u16 func_000072AC(u16 start);
static u16 func_00007300(void);
static u16 func_0000735C(u16 flags, void *fn, s16 x, s16 y, u16 x2, u16 y2, u16 z);
static u16 func_0000745C(u16 flags, void *fn, s16 x, s16 y, u16 x2, u16 y2, u16 z);
void boot_resource_global_handle_slot_record_prepare(void) {
    D_800AF0B0 = D_80173B60(D_800AF0B0);
}
static u16 func_00007258(void) {
    u16 i;
    for (i = 0; i < 6; i++) {
        if (!(g_state_slot_records[i].flags & 0x8000)) {
            return i;
        }
    }
    for (;;) {
    }
}
static u16 func_000072AC(u16 start) {
    u16 i;
    for (i = start + 1; i < 6; i++) {
        if (!(g_state_slot_records[i].flags & 0x8000)) {
            return i;
        }
    }
    for (;;) {
    }
}
static u16 func_00007300(void) {
    s16 i;
    for (i = 4; i >= 0; i--) {
        if (g_state_slot_records[i].flags & 0x8000) {
            return i + 1;
        }
    }
    for (;;) {
    }
}
static u16 func_0000735C(u16 flags, void *fn, s16 x, s16 y, u16 x2, u16 y2, u16 z) {
    u16 i = SceneFindFree();
    g_state_slot_records[i].flags = flags;
    g_state_slot_records[i].owner = fn;
    g_state_slot_records[i].x = x;
    g_state_slot_records[i].y = y;
    g_state_slot_records[i].x2 = x2;
    g_state_slot_records[i].y2 = y2;
    g_state_slot_records[i].unk0E = z;
    g_state_slot_records[i].parent = D_800C4C20;
    *(u16 *)g_state_slot_records[i].unkA4 = D_800E810E;
    return i;
}
static u16 func_0000745C(u16 flags, void *fn, s16 x, s16 y, u16 x2, u16 y2, u16 z) {
    u16 i = SceneFindAfterLast();
    g_state_slot_records[i].flags = flags;
    g_state_slot_records[i].owner = fn;
    g_state_slot_records[i].x = x;
    g_state_slot_records[i].y = y;
    g_state_slot_records[i].x2 = x2;
    g_state_slot_records[i].y2 = y2;
    g_state_slot_records[i].unk0E = z;
    g_state_slot_records[i].parent = D_800C4C20;
    *(u16 *)g_state_slot_records[i].unkA4 = D_800E810E;
    return i;
}
