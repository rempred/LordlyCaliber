typedef unsigned char u8;
typedef struct {
    u8 pad0[0xC];
    void *framebuffer;
} NUScTaskFb;
void osViSwapBuffer(void *frameBufPtr);
void func_0001b510(NUScTaskFb *gfxTask) {
    osViSwapBuffer(gfxTask->framebuffer);
}
