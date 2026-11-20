typedef signed short s16;
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
typedef struct NUScClient_s {
    struct NUScClient_s *next;
    OSMesgQueue *msgQ;
    s16 msgType;
} NUScClient;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void func_00019454(NUScClient *c, OSMesgQueue *mq, s16 msgType);
void func_00019524(NUScClient *c);
void func_00019e30(u32 retrace_num) {
    NUScClient client;
    OSMesgQueue mesgQ;
    OSMesg mesgBuf;
    osCreateMesgQueue(&mesgQ, &mesgBuf, 1);
    func_00019454(&client, &mesgQ, 1);
    while (retrace_num) {
        func_00023AE0(&mesgQ, ((void *)0), 1);
        retrace_num--;
    }
    func_00019524(&client);
}
