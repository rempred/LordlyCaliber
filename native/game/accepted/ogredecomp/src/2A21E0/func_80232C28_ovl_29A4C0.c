typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile s32 vs32;
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
extern Rec2A *D_802395B0_ovl_29A4C0;
extern u8 *D_8022A974;
void *HMalloc(s32 size);
void bzero(void *p, s32 n);
void HFree(void *p);
void nuGfxTaskAllEndWait(void);
Rec2A *func_80232C28_ovl_29A4C0(s32 id, s32 kind);
void func_801BFBA0(void *p);
s32 ChunkSize(s32 id);
void *LoadChunkRaw(void *dst, s32 id);
void *LoadChunk(s32 id);
void _bcopy(void *src, void *dst, s32 n);
void *func_80071C04(s32 size);
s32 ReadU32BE(void *p);
void DecodeLZ(void *dst, void *src);
s32 func_801C2304(void);
void *func_801C15E0(u8 *img, u32 pal, u8 *palData, u8 *out, s32 flag);
u32 func_801BE9A8(s32 siz, u32 w);
void func_80080998(u8 *dst, u8 *src, u32 n);
void func_802329B0_ovl_29A4C0(void);
void func_802329E0_ovl_29A4C0(s32 keep);
extern inline void Rec2AFree(s32 id, s32 kind) {
    Rec2A *r;
    u32 i;
    u32 k;
    if (D_802395B0_ovl_29A4C0 == ((void *)0)) {
        return;
    }
    r = D_802395B0_ovl_29A4C0;
    for (i = 0; i < 40; r++, i++) {
        if (r->id == id && r->kind == kind) {
            for (k = 0; k < r->n; k++) {
                if (r->list[k] != ((void *)0)) {
                    HFree(r->list[k]);
                    r->list[k] = ((void *)0);
                }
            }
            HFree(r->age);
            HFree(r->p08);
            HFree((void *)r->w2C);
            HFree((void *)r->w28);
            HFree(r->p38);
            HFree(r->p14);
            bzero(r, 0x3C);
            return;
        }
    }
}
void func_80232B20_ovl_29A4C0(s32 id, s32 kind);
Rec2A *func_80232C28_ovl_29A4C0(s32 id, s32 kind) {
    Rec2A *r = D_802395B0_ovl_29A4C0;
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
    tbl = HMalloc(ChunkSize(0x109F95E));
    LoadChunkRaw(tbl, 0x109F95E);
    r->chunk = tbl[r->id - 1];
    r->p08 = HMalloc(ChunkSize(r->chunk));
    LoadChunkRaw(r->p08, r->chunk);
    HFree(tbl);
    r->w2C = (s32)LoadChunk(r->p08[0]);
    r->w28 = (s32)LoadChunk(r->p08[1]);
    hdr = LoadChunk(r->p08[2]);
    r->p38 = LoadChunk(r->p08[3]);
    r->kind = kind;
    r->n = hdr[0];
    b = (r->n + 7) & ~7;
    a = (r->n * 4 + 7) & ~7;
    m = HMalloc(b * 3 + a);
    r->p14 = m;
    r->list = (void **)m;
    r->w30 = m + a;
    r->p24 = m + a + b;
    r->p34 = m + a + b + b;
    r->age = HMalloc(a);
    bzero(r->age, a);
    bzero(r->list, a);
    bzero(r->p24, b);
    _bcopy(hdr + 8, r->w30, r->n);
    _bcopy((u8 *)hdr + hdr[7], r->p34, r->n);
    HFree(hdr);
    return r;
}
u8 func_80232EA8_ovl_29A4C0(s32 id, s32 k);
void *func_80232EE8_ovl_29A4C0(s32 id, u32 *idx, u8 *outPal, s32 side, u8 kind);
void func_80233134_ovl_29A4C0(s32 id, s32 w, s32 kind);
void func_802332A0_ovl_29A4C0(void);
void func_80233398_ovl_29A4C0(void);
void func_80233448_ovl_29A4C0(void);
