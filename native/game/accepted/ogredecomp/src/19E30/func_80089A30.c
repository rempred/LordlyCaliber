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
typedef struct NUScClient_s {
               struct NUScClient_s *next;
               OSMesgQueue *msgQ;
               s16 msgType;
} NUScClient;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void func_80089054(NUScClient *c, OSMesgQueue *mq, s16 msgType);
void func_80089124(NUScClient *c);
void func_80089A30(u32 retrace_num) {
    NUScClient client;
    OSMesgQueue mesgQ;
    OSMesg mesgBuf;
    osCreateMesgQueue(&mesgQ, &mesgBuf, 1);
    func_80089054(&client, &mesgQ, 1);
    while (retrace_num) {
        osRecvMesg(&mesgQ, ((void *)0), 1);
        retrace_num--;
    }
    func_80089124(&client);
}
