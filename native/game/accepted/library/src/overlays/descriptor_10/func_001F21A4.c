typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    u8 unk0[4];
    s16 flag;
} Opt1F;
extern Opt1F D_801CE900[];
extern u8 D_801939CC;
s32 D_8016FEF4(s32 flag);
inline Opt1F *func_001F21A4(s32 i) {
    if (D_8016FEF4(D_801CE900[i].flag) && ((D_801939CC >> i) & 1)) {
        return &D_801CE900[i];
    }
    return ((void *)0);
}
