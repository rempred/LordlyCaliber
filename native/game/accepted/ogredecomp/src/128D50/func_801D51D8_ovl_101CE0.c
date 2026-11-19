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
    u8 unk0;
    u8 flags;
    u8 rest[0x17];
} SquadRec;
typedef struct {
    u8 unk0[4];
    u8 squad;
} MapUnit12;
extern SquadRec gSquads[];
f32 func_801D3910_ovl_101CE0(u8 *o);
void func_801D427C_ovl_101CE0(u8 *o);
void func_801B2B78_ovl_101CE0(u8 *o);
u8 func_801D4928_ovl_101CE0(void *sq);
u8 func_8016B088(u8 a, u8 b, u8 c, u8 d, u8 e);
void func_801D4610_ovl_101CE0(u8 *o);
void func_801D469C_ovl_101CE0(u8 *o);
void func_801D4738_ovl_101CE0(void);
u8 func_801D4A74_ovl_101CE0(u8 *sq);
void func_801D51D8_ovl_101CE0(MapUnit12 *u) {
    SquadRec *s = &gSquads[u->squad];
    s->flags &= ~4;
}
void func_801D5208_ovl_101CE0(MapUnit12 *u);
