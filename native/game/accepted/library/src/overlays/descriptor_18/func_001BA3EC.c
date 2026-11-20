typedef signed int s32;
extern s32 *D_8022A214;
void D_80079618(s32 handle, s32 flags);
extern s32 D_8022A210;
void *func_0000978C(s32 file, s32 id, s32 a, s32 b);
void func_001BA3EC(s32 id) {
    if (D_8022A210 != id) {
        if (D_8022A210 != -1) {
            ((void (*)(s32, s32, s32))D_80079618)(0x21C3C38, D_8022A210, -0x11);
        }
        D_8022A210 = id;
    }
    if (id != -1) {
        D_8022A214 = func_0000978C(0x21C3C38, id, -4, 0);
    } else {
        D_8022A214 = ((void *)0);
    }
}
