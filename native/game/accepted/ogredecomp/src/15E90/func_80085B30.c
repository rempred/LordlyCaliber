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
               u8 *base;
               u8 *cur;
               s32 len;
               s32 count;
} MusHeap;
extern MusHeap D_800B1A70;
extern u32 D_800B1A80;
extern u32 D_800B1A84;
extern u32 D_800B1A88;
extern u32 D_800B1A8C;
extern s32 D_800A9BA0;
void *alHeapDBAlloc(u8 *file, s32 line, void *hp, s32 num, s32 size);
void alHeapInit(MusHeap *hp, u8 *base, s32 len);
void func_80085B30(void *dest, u8 value, u32 length);
void func_80085A90(u8 *heap, s32 length);
void *__MusIntMemMalloc(s32 size);
s32 func_80085B0C(void);
MusHeap *func_80085B24(void);
void func_80085B30(void *dest, u8 value, u32 length) {
    u8 *p = dest;
    while (length--) {
        *p++ = value;
    }
}
void func_80085B5C(u8 *dest, u8 *src, u32 length);
u32 func_80085C74(u32 samples);
