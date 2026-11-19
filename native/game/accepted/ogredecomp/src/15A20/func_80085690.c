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
} OSTask_t;
typedef struct OSScTask_s {
               struct OSScTask_s *next;
               u32 state;
               u32 flags;
               void *framebuffer;
               OSTask_t list;
               OSMesgQueue *msgQ;
               OSMesg msg;
               u64 startTime;
               u64 totalTime;
} OSScTask;
typedef struct {
               s16 type;
               s8 misc[30];
} OSScMsg;
typedef struct OSScClient_s {
               struct OSScClient_s *next;
               OSMesgQueue *msgQ;
} OSScClient;
typedef struct {
               OSScClient client;
               OSMesgQueue msgQ;
               OSMesg msgbuf[4];
               OSMesgQueue taskQ;
               OSMesg taskbuf[4];
} MusSchedData;
typedef struct {
               u64 *data;
               u32 size;
               u64 *ucode;
               u64 *ucode_data;
} MusTask;
extern void *D_800B18A0;
extern MusSchedData *D_800B18A4;
extern u64 D_8009ECB0[];
extern u64 D_8009ED80[];
void *__MusIntMemMalloc(s32 size);
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
void osScAddClient(void *sc, OSScClient *c, OSMesgQueue *msgQ);
OSMesgQueue *osScGetCmdQ(void *sc);
void func_80085620(void *sched);
void __OsSchedInstall(void);
void func_80085690(void) {
    OSScMsg *msg;
    do {
        osRecvMesg(&D_800B18A4->msgQ, (OSMesg *)&msg, 1);
        osRecvMesg(&D_800B18A4->msgQ, ((void *)0), 0);
    } while (msg->type != 1);
}
void func_800856F0(MusTask *task);
