typedef unsigned char u8;
typedef struct {
    u8 unk0[0xC];
    void *data;
} Res2A3;
typedef struct {
    u8 unk0000[0x1CA4];
    Res2A3 *res;
} Battle2A3;
extern Battle2A3 *D_8022A974;
void *func_000016C4(void *p);
void func_002A4680(void);
void func_002A4680(void) {
    Res2A3 *r = D_8022A974->res;
    if (r != ((void *)0)) {
        func_000016C4(r->data);
        func_000016C4(r);
        D_8022A974->res = ((void *)0);
    }
}
