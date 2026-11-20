typedef unsigned char u8;
typedef signed int s32;
void D_801D9FBC(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
void D_801D9F60(s32 a, s32 b, s32 c, s32 d);
s32 func_00222E50(u8 *o, s32 a, u8 b);
void func_00223874(u8 *o, s32 a, s32 b, s32 c, u8 d) {
    s32 e;
    D_801D9FBC(0x18, b, (s32)o, 0xFF, c >> 8, c & 0xFF);
    e = b + a;
    D_801D9F60(0x19, e, (s32)o, 0xFF);
    (*(s32 *)((u8 *)(o) + (0x94))) = func_00222E50(o, e, d);
}
