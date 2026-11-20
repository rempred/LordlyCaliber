typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 unk00[0x40];
    s32 flags;
    u8 unk44[0x50];
    s32 w94;
} Msg21;
void D_801D9FBC(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
void func_0022472C(Msg21 *m, s32 a, s32 b, s32 d, s32 e) {
    D_801D9FBC(0x36, b, (s32)m, d, e >> 8, e & 0xFF);
    m->w94 = b + a;
}
