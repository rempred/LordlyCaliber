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
extern OSMesgQueue D_800B9C40;
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
u32 __osSpGetStatus(void);
void osSpTaskLoad(OSTask *task);
void osSpTaskStartGo(OSTask *task);
void func_00019758(void *arg);
void func_00019758(void *arg) {
    NUScTask *task;
    while (1) {
        func_00023AE0(&D_800B9C40, (OSMesg *)&task, 1);
        while (!(__osSpGetStatus() & 1)) {
        }
        D_800E8B10.curGraphicsTask = task;
        if (task != ((void *)0)) {
            osSpTaskLoad(&task->list);
            osSpTaskStartGo(&task->list);
        }
    }
}
