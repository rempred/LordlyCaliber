typedef unsigned short u16;
void func_000fd4f8(u16 *p) {
    if (*p == 1) {
        *p = 0;
    } else if (*p == 2) {
        *p = 0xFFFF;
    }
}
