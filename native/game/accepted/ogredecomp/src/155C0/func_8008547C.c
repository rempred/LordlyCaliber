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
} OSIoMesgHdr;
typedef struct {
               OSIoMesgHdr hdr;
               void *dramAddr;
               u32 devAddr;
               u32 size;
               void *piHandle;
} OSIoMesg;
typedef struct DmaBuffer {
               struct DmaBuffer *prev;
               struct DmaBuffer *next;
               s32 frames;
               u32 startAddr;
               u8 *ptr;
} DmaBuffer;
extern DmaBuffer *D_800B1860;
extern DmaBuffer *D_800B1864;
extern DmaBuffer *D_800B1868;
extern OSIoMesg *D_800B186C;
extern OSMesg *D_800B1870;
extern u32 D_800B1874;
extern s32 D_800B1878;
extern OSMesgQueue D_800B1880;
extern void *D_800B1898;
extern void *D_800C4B48;
extern u32 D_800E7A14;
void *__MusIntMemMalloc(s32 size);
void func_80085B30(void *dest, u8 value, u32 length);
void *osCartRomInit(void);
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
u32 osVirtualToPhysical(void *addr);
s32 osEPiStartDma(void *handle, OSIoMesg *mb, s32 direction);
DmaBuffer *func_8008547C(u32 addr, s32 len);
void *func_80085404(void *state);
s32 func_80085410(u32 addr, s32 len, void *state);
void *func_800851C0(s32 count, s32 length);
void func_80085334(void);
void *func_80085404(void *state);
s32 func_80085410(u32 addr, s32 len, void *state);
DmaBuffer *func_8008547C(u32 addr, s32 len) {
    void *handle;
    DmaBuffer *dmaPtr, *lastDmaPtr, *newPtr;
    OSIoMesg *iomsg;
    u32 addrEnd;
    if ((addr & 0xFF000000) == 0xFF000000) {
        handle = D_800C4B48;
        addr &= 0xFFFFFF;
        addr += 0x140000;
    } else {
        if (D_800E7A14 & 1) {
            return ((void *)0);
        }
        handle = D_800B1898;
    }
    lastDmaPtr = ((void *)0);
    dmaPtr = D_800B1860;
    addrEnd = addr + len;
    while (dmaPtr != ((void *)0)) {
        if (addr < dmaPtr->startAddr) {
            break;
        }
        if (addrEnd <= dmaPtr->startAddr + D_800B1874) {
            dmaPtr->frames = 2;
            return dmaPtr;
        }
        lastDmaPtr = dmaPtr;
        dmaPtr = dmaPtr->next;
    }
    newPtr = D_800B1864;
    if (newPtr == ((void *)0)) {
        return D_800B1860;
    }
    D_800B1864 = newPtr->next;
    if (lastDmaPtr != ((void *)0)) {
        newPtr->next = lastDmaPtr->next;
        if (newPtr->next != ((void *)0)) {
            newPtr->next->prev = newPtr;
        }
        newPtr->prev = lastDmaPtr;
        lastDmaPtr->next = newPtr;
    } else {
        newPtr->prev = ((void *)0);
        newPtr->next = D_800B1860;
        if (D_800B1860 != ((void *)0)) {
            D_800B1860->prev = newPtr;
        }
        D_800B1860 = newPtr;
    }
    newPtr->startAddr = addr & ~1;
    newPtr->frames = 2;
    iomsg = &D_800B186C[D_800B1878++];
    iomsg->hdr.pri = 0;
    iomsg->hdr.retQueue = &D_800B1880;
    iomsg->dramAddr = newPtr->ptr;
    iomsg->devAddr = newPtr->startAddr;
    iomsg->size = D_800B1874;
    osEPiStartDma(handle, iomsg, 0);
    return newPtr;
}
