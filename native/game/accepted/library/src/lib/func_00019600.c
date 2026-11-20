typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned long long u64;
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
extern NUSched D_800E8B10;
extern u8 D_800C4800;
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
s32 func_00023C10(OSMesgQueue *mq, OSMesg msg, s32 flag);
void osWritebackDCacheAll(void);
u32 __osSpGetStatus(void);
void osSpTaskYield(void);
s32 osSpTaskYielded(OSTask *task);
void osSpTaskLoad(OSTask *task);
void osSpTaskStartGo(OSTask *task);
void func_00019600(void *arg);
void func_00019600(void *arg) {
    NUScTask *gfxTask;
    NUScTask *audioTask;
    OSMesg msg;
    u32 yieldFlag;
    while (1) {
        func_00023AE0(&D_800E8B10.audioRequestMQ, (OSMesg *)&audioTask, 1);
        if (!(D_800C4800 & 2)) {
            osWritebackDCacheAll();
            yieldFlag = 0;
            gfxTask = D_800E8B10.curGraphicsTask;
            if ((__osSpGetStatus() & 0x202) != 0x202 && gfxTask) {
                osSpTaskYield();
                yieldFlag = 2;
                func_00023AE0(&D_800E8B10.rspMQ, &msg, 1);
                if (osSpTaskYielded(&gfxTask->list)) {
                    yieldFlag = 1;
                }
            }
            D_800E8B10.curAudioTask = audioTask;
            osSpTaskLoad(&audioTask->list);
            osSpTaskStartGo(&audioTask->list);
            func_00023AE0(&D_800E8B10.rspMQ, &msg, 1);
            D_800E8B10.curAudioTask = ((void *)0);
            if (yieldFlag == 1) {
                osSpTaskLoad(&gfxTask->list);
                osSpTaskStartGo(&gfxTask->list);
            } else if (yieldFlag == 2) {
                func_00023C10(&D_800E8B10.rspMQ, &msg, 1);
            }
        }
        func_00023C10(audioTask->msgQ, audioTask->msg, 1);
    }
}
