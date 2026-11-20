typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct { u8 b[0x76]; } ScriptObj;
extern ScriptObj D_8022A2E8[];
void bzero(void *p, s32 size);
void func_001BDBBC();
void func_000016C4(void *p);
u8 *D_8017C074(u16 a, u8 b, void *fn, s16 c, s16 d, s16 e, s16 f);
extern void *D_8022A260[];
s32 D_8009DAF4(s32 id);
void *D_8009DBB8(void *dst, s32 id);
void func_001BDBBC(void);
void func_001BDD70(void) {
    s32 n;
    s32 *tbl;
    s32 i;
    bzero(D_8022A260, 0x88);
    bzero(D_8022A2E8, 0x10);
    i = D_8009DAF4(0x21BFEDC);
    n = 34;
    if (i / 4 != n) {
        for (;;);
    }
    tbl = D_8009DBB8(((void *)0), 0x21BFEDC);
    for (i = 0; i < n; i++) {
        D_8022A260[i] = D_8009DBB8(((void *)0), tbl[i]);
    }
    func_000016C4(tbl);
    D_8017C074(0x5100, 0x13, func_001BDBBC, 0, 0, 0, 0);
}
