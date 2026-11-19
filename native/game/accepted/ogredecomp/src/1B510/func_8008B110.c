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
               u8 pad0[0xC];
               void *framebuffer;
} NUScTaskFb;
void osViSwapBuffer(void *frameBufPtr);
void func_8008B110(NUScTaskFb *gfxTask) {
    osViSwapBuffer(gfxTask->framebuffer);
}
