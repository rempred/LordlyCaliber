typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
typedef struct {
    void *data;
    s16 h4;
} Grid29;
typedef struct {
    u8 unk0[8];
    s16 h8;
    s16 hA;
} Pair29;
typedef struct {
    u8 unk0000[0x168];
    void *p168[0x38];
    Pair29 *p248[0x5BF];
    void *p1944[0x1C];
    u8 unk19B4[0x94];
    Grid29 *grid;
    u8 unk1A4C[0x168];
    void *p1BB4[20];
} Battle29;
extern Battle29 *D_8022A974;
void func_0029C5A0(s32 i, s32 v) {
    s32 end;
    u8 *u;
    if (i == -1) {
        i = 0;
        end = 28;
    } else {
        end = i + 1;
    }
    for (; i < end; i++) {
        u = (*(u8 * *)((u8 *)(D_8022A974) + (0x18 + i * 4)));
        if (u != ((void *)0)) {
            if (v != -1) {
                (*(f32 *)((u8 *)(u) + (0x120))) = v;
            }
            (*(s32 *)((u8 *)(u) + (0x128))) = 0;
            u[0x145] = (u[0x145] & ~1) | 4;
        }
    }
}
