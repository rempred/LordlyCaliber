typedef unsigned char u8;
typedef unsigned int u32;
void *func_000016C4(void *p);
void func_001FB14C(u8 *p) {
    if (p != ((void *)0)) {
        if (p[0] == 'B' && p[1] == '5') {
            u8 *bank = p;
            u8 *e = *(u8 **)(p + 0x14);
            u32 i;
            for (i = 1; i < bank[3]; i++) {
                u8 *cur = e;
                e = *(u8 **)(cur + 0xC);
                func_000016C4(cur);
            }
        }
        func_000016C4(p);
    }
}
