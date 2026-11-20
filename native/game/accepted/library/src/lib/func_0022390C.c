typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 unk00[0x40];
    s32 flags;
    u8 unk44[0x50];
    s32 w94;
} Msg21;
void D_801D9FBC(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
void func_0022390C(Msg21 *m, s32 a, s32 b, s32 c) {
    D_801D9FBC(0x1A, b, (s32)m, 0xFF, b + 100, c);
    m->w94 = b + a;
    m->flags |= 8;
}
