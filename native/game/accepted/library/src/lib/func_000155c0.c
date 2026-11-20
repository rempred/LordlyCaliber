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
void *func_00015edc(s32 size);
void func_00015f30(void *dest, u8 value, u32 length);
void *func_0001c130(void);
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
void *func_00015804(void *state);
void *func_000155c0(s32 count, s32 length) {
    s32 i;
    DmaBuffer *b;
    D_800B1898 = func_0001c130();
    D_800B186C = func_00015edc(count * 2 * sizeof(OSIoMesg));
    D_800B1870 = func_00015edc(count * 2 * sizeof(OSMesg));
    D_800B1868 = func_00015edc(count * sizeof(DmaBuffer));
    func_00015f30(D_800B1868, 0, count * sizeof(DmaBuffer));
    for (i = 0; i < count - 1; i++) {
        b = (DmaBuffer *)(i * sizeof(DmaBuffer) + (u32)D_800B1868);
        b->next = b + 1;
        b[1].prev = b;
        D_800B1868[i].ptr = func_00015edc(length);
        D_800B1868[i].startAddr = -1;
    }
    D_800B1868[i].ptr = func_00015edc(length);
    D_800B1868[i].startAddr = -1;
    D_800B1874 = length;
    D_800B1878 = 0;
    D_800B1860 = ((void *)0);
    D_800B1864 = D_800B1868;
    osCreateMesgQueue(&D_800B1880, D_800B1870, count * 2);
    return func_00015804;
}
void *func_00015804(void *state);
