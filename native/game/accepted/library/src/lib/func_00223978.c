typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 unk00[0x40];
    s32 flags;
    u8 unk44[0x50];
    s32 w94;
} Msg21;
void D_801D9F60(s32 a, s32 b, s32 c, s32 d);
void func_00223978(Msg21 *m, s32 a, s32 b) {
    D_801D9F60(0x25, b, (s32)m, 0xFF);
    m->w94 = b + a;
    m->flags |= 4;
}
