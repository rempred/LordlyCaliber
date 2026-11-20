typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void func_0004196c(u8 a, u8 b);
extern struct { u8 v; u8 pad[55]; } D_80193BD1[];
extern u8 D_8018F320;
extern u8 D_8018F321;
extern s16 D_8018F322;
u8 func_00045e5c(u8 a, u8 b);
void func_00048510(u8 i);
void func_00048928(s32 mode, s32 kind, s32 val, u8 extra) {
    u8 *st = &D_8018F320;
    u16 i;
    *st = 0;
    if (mode == 1) {
        for (i = 0; i < 100; i++) {
            if (func_00045e5c(D_80193BD1[i].v, val) == 1) {
                func_00048510(i);
                func_0004196c(0, i);
            }
        }
    } else {
        if (kind == 0) {
            *st = 1;
        } else if (kind == 2) {
            *st = 4;
        } else {
            *st = 3;
        }
        D_8018F322 = val;
        D_8018F321 = extra;
    }
}
