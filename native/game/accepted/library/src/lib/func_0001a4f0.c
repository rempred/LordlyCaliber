typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
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
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void osInvalDCache(void *addr, s32 size);
void osWritebackDCache(void *addr, s32 size);
s32 D_8008BC40(void *handle, OSIoMesg *mb, s32 direction);
void func_0001a4f0(u32 rom_addr, void *buf_ptr, u32 size, s32 direction) {
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
    D_8008BC40(D_800E79AC, &dmaIoMesgBuf, direction);
    func_00023AE0(&dmaMesgQ, &dmaMesgBuf, 1);
}
