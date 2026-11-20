typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_80190F50;
u8 *func_00050c3c(s32 id);
s32 func_00050d20(s32 id);
s32 func_000507e8(s32 id) {
    if (id >= 0x1000) {
        return 0;
    }
    if ((D_80190F50[id >> 3] >> (id & 7)) & 1) {
        return (s32)func_00050c3c(id);
    }
    return func_00050d20(id);
}
u8 *func_00050c3c(s32 id);
