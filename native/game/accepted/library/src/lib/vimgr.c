typedef struct Thread Thread;
typedef struct MesgQueue MesgQueue;
typedef struct DevMgrView {
    int active;
    Thread *thread;
    MesgQueue *commandQueue;
    MesgQueue *eventQueue;
    MesgQueue *accessQueue;
    void *dma;
    void *epiDma;
} DevMgrView;
typedef struct ViMessageView {
    unsigned short type;
    unsigned char priority;
    unsigned char status;
    MesgQueue *returnQueue;
    unsigned char remaining[16];
} ViMessageView;
typedef struct ViContextView {
    unsigned short state;
    unsigned short retraceCount;
    void *framebuffer;
    void *mode;
    unsigned int control;
    MesgQueue *queue;
    void *message;
} ViContextView;
extern DevMgrView D_800AA7C0;
extern Thread D_800C35B8;
extern unsigned char D_800C3770[4096];
extern MesgQueue D_800C4770;
extern void *D_800C4788[5];
extern ViMessageView D_800C47A0, D_800C47B8;
extern unsigned short D_800C35B0;
extern unsigned int __additional_scanline, __osViIntrCount, __osBaseCounter;
extern unsigned long long __osCurrentTime;
extern void __osTimerServicesInit(void);
extern void osCreateMesgQueue(MesgQueue *, void **, int);
extern void osSetEventMesg(int, MesgQueue *, void *);
extern int osGetThreadPri(Thread *);
extern void osSetThreadPri(Thread *, int);
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int);
extern void osCreateThread(Thread *, int, void (*)(void *), void *, void *, int);
extern void __osViInit(void);
extern void osStartThread(Thread *);
extern ViContextView *__osViGetCurrentContext(void);
extern int osRecvMesg(MesgQueue *, void **, int);
extern int osSendMesg(MesgQueue *, void *, int);
extern void __osViSwapContext(void);
extern unsigned int osGetCount(void);
extern void __osTimerInterrupt(void);
void func_000257c0(void *argument);

void func_00025620(int priority)
{
    int oldPriority;
    int currentPriority;
    unsigned int mask;
    if (D_800AA7C0.active)
        return;
    __osTimerServicesInit();
    __additional_scanline = 0;
    osCreateMesgQueue(&D_800C4770, D_800C4788, 5);
    D_800C47A0.type = 13;
    D_800C47A0.priority = 0;
    D_800C47A0.returnQueue = 0;
    D_800C47B8.type = 14;
    D_800C47B8.priority = 0;
    D_800C47B8.returnQueue = 0;
    osSetEventMesg(7, &D_800C4770, &D_800C47A0);
    osSetEventMesg(3, &D_800C4770, &D_800C47B8);
    oldPriority = -1;
    currentPriority = osGetThreadPri(0);
    if (currentPriority < priority) {
        oldPriority = currentPriority;
        osSetThreadPri(0, priority);
    }
    mask = __osDisableInt();
    D_800AA7C0.active = 1;
    D_800AA7C0.thread = &D_800C35B8;
    D_800AA7C0.commandQueue = &D_800C4770;
    D_800AA7C0.eventQueue = &D_800C4770;
    D_800AA7C0.accessQueue = 0;
    D_800AA7C0.dma = 0;
    D_800AA7C0.epiDma = 0;
    osCreateThread(&D_800C35B8, 0, func_000257c0, &D_800AA7C0,
                   D_800C3770 + sizeof(D_800C3770), priority);
    __osViInit();
    osStartThread(&D_800C35B8);
    __osRestoreInt(mask);
    if (oldPriority != -1)
        osSetThreadPri(0, oldPriority);
}

void func_000257c0(void *argument)
{
    DevMgrView *manager;
    ViMessageView *message = 0;
    ViContextView *context;
    unsigned int first;
    unsigned int count;
    D_800C35B0 = __osViGetCurrentContext()->retraceCount;
    if (D_800C35B0 == 0)
        D_800C35B0 = 1;
    first = 0;
    manager = argument;
    for (;;) {
        osRecvMesg(manager->eventQueue, (void **)&message, 1);
        switch (message->type) {
        case 13:
            __osViSwapContext();
            if (--D_800C35B0 == 0) {
                context = __osViGetCurrentContext();
                if (context->queue)
                    osSendMesg(context->queue, context->message, 0);
                D_800C35B0 = context->retraceCount;
            }
            __osViIntrCount++;
            if (first) {
                first = 0;
                count = osGetCount();
                __osCurrentTime = count;
            }
            count = __osBaseCounter;
            __osBaseCounter = osGetCount();
            count = __osBaseCounter - count;
            __osCurrentTime += count;
            break;
        case 14:
            __osTimerInterrupt();
            break;
        }
    }
}
