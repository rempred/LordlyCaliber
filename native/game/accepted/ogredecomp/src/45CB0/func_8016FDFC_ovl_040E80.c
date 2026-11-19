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
void func_8016FDFC_ovl_040E80(u8 id) {
    s16 i;
    for (i = 0; i < 121; i++) {
        u8 *e = D_80197810_ovl_040E80[i];
        if (e[0] == 0xFF) {
            e[0] = id;
            *(u16 *)(e + 2) = gDate[0];
            e[4] = ((u8 *)gDate)[2];
            e[5] = ((u8 *)gDate)[3];
            return;
        }
    }
}
void ProfileFlagSet(s32 flag, s32 value);
s32 ProfileFlagTest(s16 flag);
s32 func_8016FF5C_ovl_040E80(u8 a, u8 b);
void func_80170030_ovl_040E80(void);
