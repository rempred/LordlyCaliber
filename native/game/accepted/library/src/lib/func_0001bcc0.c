typedef struct Thread Thread;
typedef struct MesgQueue MesgQueue;
typedef struct PiHandleView PiHandleView;
typedef struct DevMgrView {
    int active;
    Thread *thread;
    MesgQueue *commandQueue;
    MesgQueue *eventQueue;
    MesgQueue *accessQueue;
    int (*dma)(int, unsigned int, void *, unsigned int);
    int (*epiDma)(PiHandleView *, int, unsigned int, void *, unsigned int);
} DevMgrView;
extern DevMgrView D_800AA400;
extern Thread D_800C2380;
extern unsigned char D_800C2530[4096];
extern MesgQueue D_800C3530, __osPiAccessQueue;
extern void *D_800C3548[1];
extern int __osPiAccessQueueEnabled;
extern void osCreateMesgQueue(MesgQueue *, void **, int);
extern void __osPiCreateAccessQueue(void);
extern void osSetEventMesg(int, MesgQueue *, void *);
extern int osGetThreadPri(Thread *);
extern void osSetThreadPri(Thread *, int);
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int);
extern void __osDevMgrMain(void *);
extern int __osPiRawStartDma(int, unsigned int, void *, unsigned int);
extern int __osEPiRawStartDma(PiHandleView *, int, unsigned int, void *, unsigned int);
extern void osCreateThread(Thread *, int, void (*)(void *), void *, void *, int);
extern void osStartThread(Thread *);

void func_0001bcc0(int priority, MesgQueue *queue, void **messages, int count)
{
    int oldPriority;
    int currentPriority;
    unsigned int mask;
    if (D_800AA400.active)
        return;
    osCreateMesgQueue(queue, messages, count);
    osCreateMesgQueue(&D_800C3530, D_800C3548, 1);
    if (!__osPiAccessQueueEnabled)
        __osPiCreateAccessQueue();
    osSetEventMesg(8, &D_800C3530, (void *)0x22222222);
    oldPriority = -1;
    currentPriority = osGetThreadPri(0);
    if (currentPriority < priority) {
        oldPriority = currentPriority;
        osSetThreadPri(0, priority);
    }
    mask = __osDisableInt();
    D_800AA400.active = 1;
    D_800AA400.thread = &D_800C2380;
    D_800AA400.commandQueue = queue;
    D_800AA400.eventQueue = &D_800C3530;
    D_800AA400.accessQueue = &__osPiAccessQueue;
    D_800AA400.dma = __osPiRawStartDma;
    D_800AA400.epiDma = __osEPiRawStartDma;
    osCreateThread(&D_800C2380, 0, __osDevMgrMain, &D_800AA400,
                   D_800C2530 + sizeof(D_800C2530), priority);
    osStartThread(&D_800C2380);
    __osRestoreInt(mask);
    if (oldPriority != -1)
        osSetThreadPri(0, oldPriority);
}
