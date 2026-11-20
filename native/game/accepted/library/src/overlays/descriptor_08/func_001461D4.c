typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 flags;
    u8 unk04[0x6C];
    s32 unk70;
} MapUnit;
extern s32 D_801F367C;
extern MapUnit *D_801F0CB0[];
s32 func_001461D4(MapUnit *u) {
    s32 n = D_801F367C;
    s32 i;
    for (i = 0; i < n; i++) {
        if (D_801F0CB0[i] == u) {
            return i;
        }
    }
    for (;;) {
    }
}
