typedef signed int s32;
extern void *D_8022A4C0[];
void func_000016C4(void *p);
s32 D_8009DAF4(s32 id);
void *D_8009DBB8(void *dst, s32 id);
void func_001BDE70(void) {
    s32 n = 6;
    s32 *tbl;
    s32 i;
    i = D_8009DAF4(0x21C2F70);
    if (i / 4 != n) {
        for (;;) {
        }
    }
    tbl = D_8009DBB8(((void *)0), 0x21C2F70);
    for (i = 0; i < n; i++) {
        D_8022A4C0[i] = D_8009DBB8(((void *)0), tbl[i]);
    }
    func_000016C4(tbl);
}
