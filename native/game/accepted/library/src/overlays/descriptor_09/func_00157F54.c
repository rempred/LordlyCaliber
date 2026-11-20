typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_80213778;
void func_0015DF10(s32 a);
extern u16 *D_800C4C4C;
extern u8 *D_80213778;
s8 func_00157F54(s8 cur) {
    u16 b = *D_800C4C4C;
    if (b & 0x100) {
        if (cur != 1 || D_80213778[0x5D8] != 0) {
            if (++cur == 2) {
                func_0015DF10(2);
            }
        }
    } else if (b & 0x200) {
        if (cur >= 2) {
            cur--;
            func_0015DF10(2);
        }
    }
    return cur < 0 ? 0 : (cur < 3 ? cur : 2);
}
