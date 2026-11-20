typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
void *func_001F0A9C(s32 id);
void func_001F3C00(u8 *o, s32 a, s32 b, s32 c, s32 d, s32 e);
void *func_001F0A9C(s32 id);
s32 func_001F70CC(u8 *o) {
    u8 *p = o;
    s32 v;
    if ((*(u32 *)((u8 *)(o) + (0x94))) & 4) {
        v = (s32)func_001F0A9C(0x3224BA);
    } else {
        v = 0;
    }
    func_001F3C00(o, p[0x14] != 3, (*(u32 *)((u8 *)(p) + (0x94))) & 2, v, 0, 0);
    return 1;
}
