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
extern u8 gChaosFrame;
extern u8 D_80197810_ovl_040E80[][6];
extern u16 gDate[];
extern u8 D_80193686_ovl_040E80[];
extern u8 D_80195560_ovl_040E80[][0x34];
extern u16 D_801953F4_ovl_040E80[];
extern u8 D_801971F0_ovl_040E80[][0x19];
extern u8 D_801969B8_ovl_040E80[][0xB];
void func_8016FDB0_ovl_040E80(s8 delta);
void func_8016FDFC_ovl_040E80(u8 id);
void ProfileFlagSet(s32 flag, s32 value);
s32 ProfileFlagTest(s16 flag);
s32 func_8016FF5C_ovl_040E80(u8 a, u8 b);
void func_80170030_ovl_040E80(void) {
    s16 i;
    for (i = 1; i < 99; i++) {
        bzero(D_80195560_ovl_040E80[i], 0x34);
    }
    for (i = 0; i < 120; i++) {
        D_801953F4_ovl_040E80[i] = 0;
    }
    for (i = 30; i < 50; i++) {
        bzero(D_801971F0_ovl_040E80[i], 0x19);
    }
    for (i = 0; i < 10; i++) {
        if (D_801969B8_ovl_040E80[i][1] & 0x80) {
            bzero(D_801969B8_ovl_040E80[i], 0xB);
        }
    }
}
