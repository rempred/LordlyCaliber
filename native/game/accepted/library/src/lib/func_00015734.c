typedef unsigned char u8;
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
typedef struct DmaBuffer {
    struct DmaBuffer *prev;
    struct DmaBuffer *next;
    s32 frames;
    u32 startAddr;
    u8 *ptr;
} DmaBuffer;
extern DmaBuffer *D_800B1860;
extern DmaBuffer *D_800B1864;
extern s32 D_800B1878;
extern OSMesgQueue D_800B1880;
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void func_00015734(void) {
    OSMesg msg;
    DmaBuffer *dmaPtr, *next;
    while (D_800B1878 != 0) {
        func_00023AE0(&D_800B1880, &msg, 0);
        D_800B1878--;
    }
    dmaPtr = D_800B1860;
    while (dmaPtr != ((void *)0)) {
        if (--dmaPtr->frames == 0) {
            next = dmaPtr->next;
            if (next != ((void *)0)) {
                next->prev = dmaPtr->prev;
            }
            if (dmaPtr->prev != ((void *)0)) {
                dmaPtr->prev->next = dmaPtr->next;
            } else {
                D_800B1860 = dmaPtr->next;
            }
            dmaPtr->prev = ((void *)0);
            dmaPtr->next = D_800B1864;
            D_800B1864 = dmaPtr;
            dmaPtr = next;
        } else {
            dmaPtr = dmaPtr->next;
        }
    }
}
