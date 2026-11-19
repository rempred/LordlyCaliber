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
    u64 pad[0x1B0 / 8];
} OSThread;
typedef struct {
               u32 type;
               u32 flags;
               u64 *ucode_boot;
               u32 ucode_boot_size;
               u64 *ucode;
               u32 ucode_size;
               u64 *ucode_data;
               u32 ucode_data_size;
               u64 *dram_stack;
               u32 dram_stack_size;
               u64 *output_buff;
               u64 *output_buff_size;
               u64 *data_ptr;
               u32 data_size;
               u64 *yield_data_ptr;
               u32 yield_data_size;
} OSTask;
typedef s16 NUScMsg;
typedef struct NUScClient_s {
               struct NUScClient_s *next;
               OSMesgQueue *msgQ;
               NUScMsg msgType;
} NUScClient;
typedef struct NUScTask_s {
               struct NUScTask_s *next;
               u32 state;
               u32 flags;
               void *framebuffer;
               OSTask list;
               OSMesgQueue *msgQ;
               OSMesg msg;
} NUScTask;
typedef struct {
                NUScMsg retraceMsg;
                NUScMsg prenmiMsg;
                OSMesgQueue audioRequestMQ;
                OSMesg audioRequestBuf[8];
                OSMesgQueue graphicsRequestMQ;
                OSMesg graphicsRequestBuf[8];
                OSMesgQueue retraceMQ;
                OSMesg retraceMsgBuf[8];
                OSMesgQueue rspMQ;
                OSMesg rspMsgBuf[8];
                OSMesgQueue rdpMQ;
                OSMesg rdpMsgBuf[8];
                OSMesgQueue waitMQ;
                OSMesg waitMsgBuf[8];
                OSThread schedulerThread;
                OSThread audioThread;
                OSThread graphicsThread;
                NUScClient *clientList;
                NUScTask *curGraphicsTask;
                NUScTask *curAudioTask;
                NUScTask *graphicsTaskSuspended;
                u32 retraceCount;
                u8 frameRate;
                u8 frameBufferNum;
} NUSched;
extern NUSched nusched;
extern u8 nuScPreNMIFlag;
extern u32 nuScRetraceCounter;
extern void (*nuScPreNMIFunc)(void);
extern u64 nuScStack[];
extern u64 nuScAudioStack[];
extern u64 nuScGraphicsStack[];
extern OSThread D_800B7A90;
extern OSMesgQueue D_800B9C40;
extern OSMesg D_800B9C58[];
extern u8 osViModeTable[];
extern s32 osTvType;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
void osCreateViManager(s32 pri);
void osViSetMode(void *mode);
void osViSetEvent(OSMesgQueue *mq, OSMesg msg, u32 retraceCount);
void osSetEventMesg(s32 event, OSMesgQueue *mq, OSMesg msg);
void osCreateThread(void *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(void *t);
u32 osSetIntMask(u32 mask);
void osWritebackDCacheAll(void);
u32 __osSpGetStatus(void);
void osSpTaskYield(void);
s32 osSpTaskYielded(OSTask *task);
void osSpTaskLoad(OSTask *task);
void osSpTaskStartGo(OSTask *task);
s32 osAfterPreNMI(void);
void osViSetYScale(f32 scale);
void *osViGetCurrentFramebuffer(void);
void *osViGetNextFramebuffer(void);
void func_80089BE4(u8 active);
void func_80088F08(void *arg);
void func_80089200(void *arg);
void func_800893C0(void *arg);
void func_80089358(void *arg);
void func_80089054(NUScClient *c, OSMesgQueue *mq, NUScMsg msgType);
void func_80089124(NUScClient *c);
void func_800891A0(NUScMsg *msg);
void func_8008949C(NUScTask *task);
void func_80088C50(u8 videoMode, u8 numFields);
OSMesgQueue *func_80088EF0(void);
OSMesgQueue *func_80088EFC(void);
void func_80088F08(void *arg) {
    OSMesg msg;
    s32 beforeResetFrame;
    nuScRetraceCounter = 0;
    while (1) {
        osRecvMesg(&nusched.retraceMQ, &msg, 1);
        switch ((s32)msg) {
            case 666:
                nuScRetraceCounter++;
                func_800891A0(&nusched.retraceMsg);
                if (nuScPreNMIFlag) {
                    if (beforeResetFrame) {
                        beforeResetFrame--;
                    } else {
                        nuScPreNMIFlag |= 2;
                        osAfterPreNMI();
                        osViSetYScale(1.0f);
                        func_80089BE4(1);
                    }
                }
                break;
            case 669:
                nuScPreNMIFlag = 1;
                func_800891A0(&nusched.prenmiMsg);
                if (nuScPreNMIFunc != ((void *)0)) {
                    nuScPreNMIFunc();
                }
                beforeResetFrame = nusched.frameRate / 2 / nusched.retraceCount - 3;
                break;
        }
    }
}
void func_80089054(NUScClient *c, OSMesgQueue *mq, NUScMsg msgType);
void func_800890E4(NUScClient *c, NUScMsg msgType);
void func_80089124(NUScClient *c);
void func_800891A0(NUScMsg *msg);
void func_80089200(void *arg);
void func_80089358(void *arg);
void func_800893C0(void *arg);
void func_8008949C(NUScTask *task);
void func_80089528(u8 frameBufferNum);
u8 func_80089534(void);
