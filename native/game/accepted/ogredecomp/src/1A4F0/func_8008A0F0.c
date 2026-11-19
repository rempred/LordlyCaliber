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
typedef void *OSMesg;
typedef struct {
               void *mtqueue;
               void *fullqueue;
               s32 validCount;
               s32 first;
               s32 msgCount;
               OSMesg *msg;
} OSMesgQueue;
typedef struct {
               u16 type;
               u8 pri;
               u8 status;
               OSMesgQueue *retQueue;
               void *dramAddr;
               u32 devAddr;
               u32 size;
               void *piHandle;
} OSIoMesg;
extern void *D_800E79AC;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void osInvalDCache(void *addr, s32 size);
void osWritebackDCache(void *addr, s32 size);
s32 osEPiStartDma(void *handle, OSIoMesg *mb, s32 direction);
void func_8008A0F0(u32 rom_addr, void *buf_ptr, u32 size, s32 direction) {
    OSIoMesg dmaIoMesgBuf;
    OSMesgQueue dmaMesgQ;
    OSMesg dmaMesgBuf;
    osCreateMesgQueue(&dmaMesgQ, &dmaMesgBuf, 1);
    dmaIoMesgBuf.pri = 0;
    dmaIoMesgBuf.retQueue = &dmaMesgQ;
    dmaIoMesgBuf.dramAddr = buf_ptr;
    dmaIoMesgBuf.devAddr = rom_addr;
    dmaIoMesgBuf.size = size;
    if (direction == 0) {
        osInvalDCache(buf_ptr, size);
    } else {
        osWritebackDCache(buf_ptr, size);
    }
    osEPiStartDma(D_800E79AC, &dmaIoMesgBuf, direction);
    osRecvMesg(&dmaMesgQ, &dmaMesgBuf, 1);
}
