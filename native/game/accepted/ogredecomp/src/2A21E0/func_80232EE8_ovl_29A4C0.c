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
Rec2A *func_80232C28_ovl_29A4C0(s32 id, s32 kind);
u8 func_80232EA8_ovl_29A4C0(s32 id, s32 k);
void *func_80232EE8_ovl_29A4C0(s32 id, u32 *idx, u8 *outPal, s32 side, u8 kind) {
    u32 n = *idx;
    u32 i;
    Rec2A *r;
    u8 *raw;
    u8 *img;
    u8 *pal;
    void *res;
    s32 flag;
    u32 size;
    for (i = 0; i < 40; i++) {
        r = &D_802395B0_ovl_29A4C0[i];
        if (r->id != id || r->kind != kind) {
            continue;
        }
        if (r->list[n] == ((void *)0)) {
            raw = func_80071C04(ChunkSize(r->p08[n + 4]));
            LoadChunkRaw(raw, r->p08[n + 4]);
            img = func_80071C04(ReadU32BE(raw));
            DecodeLZ(img, raw);
            HFree(raw);
            if (*(u16 *)img == 0x5554) {
                flag = 0;
                pal = (u8 *)r->p38 + (r->p34[n] << 9);
                if ((*(u8 * *)((u8 *)(D_8022A974) + (0x1A48))) != ((void *)0) &&
                    (*(s16 *)((u8 *)((*(u8 * *)((u8 *)(D_8022A974) + (0x1A48)))) + (4))) != side) {
                    flag = func_801C2304() & 1;
                }
                if ((*(u8 * *)((u8 *)(D_8022A974) + (0x1CA4))) != ((void *)0)) {
                    flag &= -(*(*(u8 * *)((u8 *)(D_8022A974) + (0x1CA4))) != 3);
                }
                res = func_801C15E0(img, kind < img[2] ? kind : 0, pal, r->p24 + n, flag);
                HFree(img);
            } else if (*(u16 *)(res = img) == 0x3634) {
                size = func_801BE9A8(((u8 *)res)[3], *(u16 *)((u8 *)res + 4)) * *(u16 *)((u8 *)res + 6) + 8;
                res = HMalloc(size);
                func_80080998(res, img, size);
            } else {
                goto done;
            }
            r->list[n] = res;
        }
    done:
        if (outPal != ((void *)0)) {
            *outPal = r->p24[n];
        }
        r->age[n] = 0;
        return r->list[n];
    }
    return ((void *)0);
}
void func_80233134_ovl_29A4C0(s32 id, s32 w, s32 kind);
void func_802332A0_ovl_29A4C0(void);
void func_80233398_ovl_29A4C0(void);
void func_80233448_ovl_29A4C0(void);
