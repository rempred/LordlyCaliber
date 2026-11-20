typedef unsigned char u8;
typedef signed int s32;
u8 func_000c2f68(void);
extern u8 D_801EF487[];
s32 func_000c2770(u8 *out) {
    u8 r;
    u8 none;
    *out = 0;
    r = func_000c2f68();
    none = r == 0;
    if (!none) {
        *out = D_801EF487[r];
    }
    return none;
}
