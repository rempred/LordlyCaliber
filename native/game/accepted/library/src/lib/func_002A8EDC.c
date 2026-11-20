typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_8022A974;
inline void func_002A8EDC(u8 a, u8 b) {
    u8 *ctx = D_8022A974;
    u8 *pa = (*(u8 * *)((u8 *)(ctx + a * 4) + (0x18)));
    u8 *pb = (*(u8 * *)((u8 *)(ctx + b * 4) + (0x18)));
    (*(u8 * *)((u8 *)(ctx + a * 4) + (0x18))) = pb;
    (*(u8 * *)((u8 *)(ctx + b * 4) + (0x18))) = pa;
    if (pa != ((void *)0)) {
        (*(s32 *)((u8 *)(pa) + (0xE4))) = b;
    }
    if (pb != ((void *)0)) {
        (*(s32 *)((u8 *)(pb) + (0xE4))) = a;
    }
}
