typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    s32 id;
    s32 chunk;
    s32 *p08;
    s32 kind;
    u32 n;
    void *p14;
    u8 unk18[4];
    void **list;
    u32 *age;
    u8 *p24;
    s32 w28;
    s32 w2C;
    u8 *w30;
    u8 *p34;
    void *p38;
} Rec2A;
extern Rec2A *D_802395B0;
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
void func_000016C4(void *p);
Rec2A *func_002A2458(s32 id, s32 kind);
s32 D_8009DAF4(s32 id);
void *D_8009DBB8(void *dst, s32 id);
void *D_8009DD38(s32 id);
void bcopy(void *src, void *dst, s32 n);
Rec2A *func_002A2458(s32 id, s32 kind) {
    Rec2A *r = D_802395B0;
    s32 i = 39;
    s32 *tbl;
    u32 *hdr;
    u32 a;
    u32 b;
    u8 *m;
    while (1) {
        if (r->id == 0) {
            break;
        }
        r++;
        if (i-- == 0) {
            break;
        }
    }
    r->id = id;
    tbl = func_00001330(D_8009DAF4(0x109F95E));
    D_8009DBB8(tbl, 0x109F95E);
    r->chunk = tbl[r->id - 1];
    r->p08 = func_00001330(D_8009DAF4(r->chunk));
    D_8009DBB8(r->p08, r->chunk);
    func_000016C4(tbl);
    r->w2C = (s32)D_8009DD38(r->p08[0]);
    r->w28 = (s32)D_8009DD38(r->p08[1]);
    hdr = D_8009DD38(r->p08[2]);
    r->p38 = D_8009DD38(r->p08[3]);
    r->kind = kind;
    r->n = hdr[0];
    b = (r->n + 7) & ~7;
    a = (r->n * 4 + 7) & ~7;
    m = func_00001330(b * 3 + a);
    r->p14 = m;
    r->list = (void **)m;
    r->w30 = m + a;
    r->p24 = m + a + b;
    r->p34 = m + a + b + b;
    r->age = func_00001330(a);
    bzero(r->age, a);
    bzero(r->list, a);
    bzero(r->p24, b);
    bcopy(hdr + 8, r->w30, r->n);
    bcopy((u8 *)hdr + hdr[7], r->p34, r->n);
    func_000016C4(hdr);
    return r;
}
