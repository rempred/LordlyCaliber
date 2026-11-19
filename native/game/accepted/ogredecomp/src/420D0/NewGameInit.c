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
void osSetTime(u64 t);
void bzero(void *p, s32 n);
typedef struct {
    u16 hours;
    u8 minutes;
    u8 seconds;
} Clock420;
typedef struct {
    u16 year;
    u8 month;
    u8 day;
} Date420;
extern Clock420 D_80196A2C_ovl_040E80;
extern u8 D_80196A30_ovl_040E80[];
extern u8 D_80196A34_ovl_040E80;
extern u8 D_80196A35_ovl_040E80;
s32 DateAdvanceDay(Date420 *d);
u64 osGetTime(void);
extern u8 D_80196A28_ovl_040E80[];
extern u8 D_80196A32_ovl_040E80;
extern u8 D_80196A33_ovl_040E80;
extern u8 D_80196A36_ovl_040E80;
extern u16 D_80193678_ovl_040E80;
extern u8 D_8019367A_ovl_040E80;
extern u8 D_8019367B_ovl_040E80;
extern u8 D_801936A7_ovl_040E80;
extern u8 D_801936A9_ovl_040E80;
extern u8 D_801936AA_ovl_040E80;
extern u8 D_801936AC_ovl_040E80;
extern u8 D_801936AD_ovl_040E80;
extern u8 D_80196AA4_ovl_040E80;
extern u8 D_80197810_ovl_040E80[];
void *memset(void *p, s32 c, s32 n);
extern u8 D_80186FFF_ovl_040E80[];
void ProfileFlagSet(s32 flag, s32 v);
extern inline s32 func_8016C1D0_ovl_040E80(u16 year, u8 era) {
    s32 r;
    if (era) {
        year += 1748;
    }
    r = 0;
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
        r = 1;
    }
    return r;
}
extern inline u8 func_8016C528_ovl_040E80(Clock420 *c, u8 mode) {
    if (++c->seconds == 60) {
        c->minutes++;
        c->seconds = 0;
        if (c->minutes == 60) {
            c->hours++;
            c->minutes = 0;
            if (mode == 1) {
                if (c->hours == 24) {
                    c->hours = 0;
                    return 1;
                }
            } else if (c->hours == 0xFFFE) {
                c->hours--;
            }
        }
    }
    return 0;
}
s32 DateAdvanceDay(Date420 *d);
s32 func_8016C70C_ovl_040E80(Date420 *d);
void func_8016C814_ovl_040E80(void);
void func_8016C81C_ovl_040E80(void);
void NewGameInit(void) {
    u16 *p;
    func_8016CB08_ovl_040E80(D_80196A28_ovl_040E80);
    p = &D_80193678_ovl_040E80;
    *(u16 *)D_80196A30_ovl_040E80 = 0xFB;
    D_80196A36_ovl_040E80 = 0;
    D_80196A32_ovl_040E80 = 4;
    D_80196A33_ovl_040E80 = 1;
    D_80196A2C_ovl_040E80.hours = 6;
    D_80196A2C_ovl_040E80.minutes = 0;
    D_80196A2C_ovl_040E80.seconds = 0;
    bzero(p, 0x3A);
    D_801936AA_ovl_040E80 = 7;
    D_801936A9_ovl_040E80 = 50;
    D_801936A7_ovl_040E80 = 0;
    *p = 0xF9;
    D_8019367A_ovl_040E80 = 1;
    D_8019367B_ovl_040E80 = 1;
    D_801936AD_ovl_040E80 = 4;
    D_801936AC_ovl_040E80 = 2;
    D_80196AA4_ovl_040E80 = 2;
    memset(D_80197810_ovl_040E80, 0xFF, 0x2D6);
}
void func_8016CB08_ovl_040E80(void *p);
void PlayTimeUpdate(Clock420 *c);
void func_8016CC2C_ovl_040E80(Clock420 *c);
