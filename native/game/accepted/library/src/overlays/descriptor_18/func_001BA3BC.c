typedef unsigned char u8;
typedef signed int s32;
extern s32 *D_8022A214;
u8 *func_001BA3BC(s32 i) {
    if (D_8022A214 == ((void *)0)) {
        for (;;) {
        }
    }
    return (u8 *)((s32)D_8022A214 + D_8022A214[i]);
}
