typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct DmaBuffer {
    struct DmaBuffer *prev;
    struct DmaBuffer *next;
    s32 frames;
    u32 startAddr;
    u8 *ptr;
} DmaBuffer;
u32 osVirtualToPhysical(void *addr);
DmaBuffer *func_0001587c(u32 addr, s32 len);
s32 func_00015810(u32 addr, s32 len, void *state);
s32 func_00015810(u32 addr, s32 len, void *state) {
    DmaBuffer *dmaPtr = func_0001587c(addr, len);
    if (dmaPtr == ((void *)0)) {
        return osVirtualToPhysical((void *)addr);
    }
    if ((addr & 0xFF000000) == 0xFF000000) {
        addr &= 0xFFFFFF;
        addr += 0x140000;
    }
    return osVirtualToPhysical(dmaPtr->ptr + addr - dmaPtr->startAddr);
}
DmaBuffer *func_0001587c(u32 addr, s32 len);
