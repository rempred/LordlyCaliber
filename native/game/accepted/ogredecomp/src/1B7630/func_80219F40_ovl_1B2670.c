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
u8 *func_80219F40_ovl_1B2670(u8 *t, u32 page);
typedef struct {
               u16 sec0;
               u16 unk02;
               u16 sec1;
               u8 unk06[0x10];
               u16 size;
} TblHdr;
extern TblHdr *D_8021C9E8_ovl_1B2670;
extern u8 *D_8021C9E0_ovl_1B2670;
extern u8 *D_8021C9E4_ovl_1B2670;
s32 ChunkSize(s32 id);
void *LoadChunkRaw(void *dst, s32 id);
void *HMalloc(s32 size);
void HFree(void *p);
void bzero(void *p, s32 n);
void _bcopy(const void *src, void *dst, s32 n);
void *func_8021B5EC_ovl_1B2670(s32 a, s32 b, s32 c, s32 d, s32 e);
u8 *func_80219F40_ovl_1B2670(u8 *t, u32 page) {
    u32 w;
    s32 h;
    u32 pix;
    u32 sub;
    u32 off;
    u32 i;
    u8 *e;
    u8 f;
    u8 g;
    if (t == ((void *)0)) {
        return ((void *)0);
    }
    f = t[3];
    w = *(u16 *)(t + 4);
    h = *(u16 *)(t + 6);
    if (f & 2) {
        pix = ((w + 3) >> 2 << 4) * h;
        sub = 0;
    } else if (f & 4) {
        pix = ((w + 7) >> 3 << 3) * h;
        if (f & 1) {
            sub = ((w + 15) >> 4 << 3) * h;
        } else {
            sub = pix;
        }
    } else {
        pix = ((w + 3) >> 2 << 3) * h;
        if (f & 1) {
            sub = ((w + 15) >> 4 << 3) * h;
        } else {
            sub = ((w + 7) >> 3 << 3) * h;
        }
    }
    off = 8;
    e = t + 8;
    for (i = 0; i < t[2]; i++) {
        if (i == page) {
            return e;
        }
        g = e[3];
        off += 8;
        if (g & 1) {
            off += pix;
        }
        if (g & 2) {
            off += sub;
        }
        if (g & 4) {
            off += 0x200;
        }
        e = t + off;
    }
    return ((void *)0);
}
u8 *func_8021A064_ovl_1B2670(u8 *t, u32 page);
u8 *func_8021A110_ovl_1B2670(u8 *desc, u32 index);
