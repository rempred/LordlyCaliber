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
u8 *func_80219F40_ovl_1B2670(u8 *t, u32 page);
u8 *func_8021A064_ovl_1B2670(u8 *t, u32 page);
u8 *func_8021A110_ovl_1B2670(u8 *desc, u32 index) {
    u32 w = *(u16 *)(desc + 4);
    u32 h = *(u16 *)(desc + 6);
    u8 *tex = func_80219F40_ovl_1B2670(desc, index);
    u32 off;
    if (tex == ((void *)0)) {
        return ((void *)0);
    }
    off = 8;
    if (tex[3] & 1) {
        if (desc[3] & 2) {
            off = h * (((w + 3) >> 2) << 4) + 8;
        } else if (desc[3] & 4) {
            off = h * (((w + 7) >> 3) << 3) + 8;
        } else {
            off = h * (((w + 3) >> 2) << 3) + 8;
        }
    }
    if (tex[3] & 2) {
        if (desc[3] & 1) {
            off += h * (((w + 15) >> 4) << 3);
        } else {
            off += h * (((w + 7) >> 3) << 3);
        }
    }
    if (tex[3] & 4) {
        return tex + off;
    }
    return ((void *)0);
}
