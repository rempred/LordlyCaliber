typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    u8 unk0[8];
    s16 a;
    s16 b;
} Pair28;
extern Pair28 *D_8023DE38;
void *func_00001330(s32 size);
typedef struct {
    s16 h0;
    s16 h2;
    s16 h4;
    s16 h6;
    s16 h8;
    s16 hA;
    s16 hC;
} Pair28Full;
extern u8 D_8022A980;
void func_00290C50(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f) {
    D_8022A980 = 0x80;
    if (D_8023DE38 == ((void *)0)) {
        D_8023DE38 = func_00001330(0xE);
    }
    ((Pair28Full *)D_8023DE38)->h4 = a;
    ((Pair28Full *)D_8023DE38)->h6 = b;
    ((Pair28Full *)D_8023DE38)->h0 = c;
    ((Pair28Full *)D_8023DE38)->h2 = d;
    ((Pair28Full *)D_8023DE38)->h8 = 0;
    ((Pair28Full *)D_8023DE38)->hA = e;
    ((Pair28Full *)D_8023DE38)->hC = f;
}
