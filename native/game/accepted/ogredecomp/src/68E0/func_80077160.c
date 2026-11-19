typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile s32 vs32;
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
extern Scene D_800E82C8[];
extern u16 D_800C49D0;
extern void *D_800AF0B0;
extern u16 D_800E810E;
extern u8 D_800C4C10[];
extern s32 D_800E79A0;
typedef struct {
              u16 h0;
              u8 b2;
              u8 b3;
              u8 b4;
} Pad0A0;
extern Pad0A0 D_800AF0A0;
extern u8 D_800AF0A2;
extern u8 D_800AF0A3;
extern u8 D_800AF0A4;
extern u16 D_800AF0A6[];
extern Pad0A0 *D_800AF090[];
u64 osGetTime(void);
void func_8009C7C0(u32 seed);
void bzero(void *p, s32 n);
extern void *D_800E7A68[];
extern void *D_800E7A90[];
extern signed char D_800ADF88[];
void func_800775EC(void);
void func_80077BF8(void);
void func_8007819C(void);
void func_80173BA0(void *p);
void *func_80173B60(void *p);
void osSyncPrintf(const signed char *fmt, ...);
void _bcopy(const void *src, void *dst, s32 n);
void HFree(void *p);
extern s32 D_800C4C20;
extern u16 D_800C4C26;
extern u8 D_800E7A32;
void func_80077F88(s32 i);
void *HMallocBack(s32 size);
void func_800764E0(void);
void func_80076DC8(void);
void func_80076E00(void);
void func_80076E2C(void);
u16 func_80076E58(void);
u16 func_80076EAC(u16 start);
u16 func_80076F00(void);
extern inline u16 SceneFindFree(void) {
    u16 i;
    for (i = 0; i < 6; i++) {
        if (!(D_800E82C8[i].flags & 0x8000)) {
            return i;
        }
    }
    for (;;);
}
extern inline u16 SceneFindAfterLast(void) {
    s16 i;
    for (i = 4; i >= 0; i--) {
        if (D_800E82C8[i].flags & 0x8000) {
            return i + 1;
        }
    }
    for (;;);
}
u16 func_80076F5C(u16 flags, void *fn, s16 x, s16 y, u16 x2, u16 y2, u16 z);
u16 func_8007705C(u16 flags, void *fn, s16 x, s16 y, u16 x2, u16 y2, u16 z);
void func_80077160(void) {
    s32 i;
    if (D_800C4C20 < 0) {
        return;
    }
    for (i = 0; i < 6; i++) {
        if (i == D_800C4C20) {
            continue;
        }
        if (!(D_800E82C8[i].flags & 0x8000)) {
            continue;
        }
        if ((D_800E82C8[i].parent) != D_800C4C20) {
            continue;
        }
        func_80077F88(i);
    }
    {
        u8 *p = &D_800E7A32;
        *p |= 2;
    }
}
void func_80077200(s32 id);
s32 func_80077314(void *owner);
s32 func_80077368(s32 scene);
s32 func_800773D4(void);
s32 func_8007740C(s32 scene);
s32 func_8007745C(void);
void func_80077F80(void);
void func_800780D4(u8 kind, void *src, u16 n, u8 **out);
void func_80078164(u8 *p, void *dst);
void func_800782EC(void);
