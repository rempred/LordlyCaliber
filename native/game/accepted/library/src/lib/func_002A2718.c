typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
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
extern u8 *D_8022A974;
void *func_00001330(s32 size);
void func_000016C4(void *p);
s32 D_8009DAF4(s32 id);
void *D_8009DBB8(void *dst, s32 id);
void *resource_alloc_tree_scan(s32 size);
s32 func_0000ABE0(void *p);
void func_0000A510(void *dst, void *src);
s32 D_801C2304(void);
void *D_801C15E0(u8 *img, u32 pal, u8 *palData, u8 *out, s32 flag);
u32 D_801BE9A8(s32 siz, u32 w);
void memcpy_aligned(u8 *dst, u8 *src, u32 n);
void *func_002A2718(s32 id, u32 *idx, u8 *outPal, s32 side, u8 kind) {
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
        r = &D_802395B0[i];
        if (r->id != id || r->kind != kind) {
            continue;
        }
        if (r->list[n] == ((void *)0)) {
            raw = resource_alloc_tree_scan(D_8009DAF4(r->p08[n + 4]));
            D_8009DBB8(raw, r->p08[n + 4]);
            img = resource_alloc_tree_scan(func_0000ABE0(raw));
            func_0000A510(img, raw);
            func_000016C4(raw);
            if (*(u16 *)img == 0x5554) {
                flag = 0;
                pal = (u8 *)r->p38 + (r->p34[n] << 9);
                if ((*(u8 * *)((u8 *)(D_8022A974) + (0x1A48))) != ((void *)0) &&
                    (*(s16 *)((u8 *)((*(u8 * *)((u8 *)(D_8022A974) + (0x1A48)))) + (4))) != side) {
                    flag = D_801C2304() & 1;
                }
                if ((*(u8 * *)((u8 *)(D_8022A974) + (0x1CA4))) != ((void *)0)) {
                    flag &= -(*(*(u8 * *)((u8 *)(D_8022A974) + (0x1CA4))) != 3);
                }
                res = D_801C15E0(img, kind < img[2] ? kind : 0, pal, r->p24 + n, flag);
                func_000016C4(img);
            } else if (*(u16 *)(res = img) == 0x3634) {
                size = D_801BE9A8(((u8 *)res)[3], *(u16 *)((u8 *)res + 4)) * *(u16 *)((u8 *)res + 6) + 8;
                res = func_00001330(size);
                memcpy_aligned(res, img, size);
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
