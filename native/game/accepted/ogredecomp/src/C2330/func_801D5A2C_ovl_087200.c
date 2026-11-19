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
extern u8 *D_80196AF8;
extern u8 D_801EF474_ovl_087200[];
extern s8 D_801EF480_ovl_087200[];
u8 func_801D6508_ovl_087200(void);
extern u8 D_801EF487_ovl_087200[];
u8 func_801D5A2C_ovl_087200(u8 m) {
    u8 *st = D_80196AF8;
    s32 n = 0;
    s32 k;
    u8 *q;
    s32 pos;
    u8 mask;
    u8 c;
    q = st + ((*(u16 *)((u8 *)(st) + (0x18C))) * 0x36 + 0x117C);
    pos = (*(u16 *)((u8 *)(st) + (0x19C)));
    mask = D_801EF474_ovl_087200[pos];
    for (k = 0; k < 8; k++) {
        if ((mask >> k) & 1) {
            c = (q + (pos + D_801EF480_ovl_087200[k]))[4];
            if (c != 0xFF && c != m) {
                n++;
                break;
            }
        }
    }
    return n;
}
s32 func_801D5D10_ovl_087200(u8 *out);
