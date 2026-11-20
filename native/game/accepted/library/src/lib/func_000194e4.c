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
typedef s16 NUScMsg;
typedef struct NUScClient_s {
    struct NUScClient_s *next;
    OSMesgQueue *msgQ;
    NUScMsg msgType;
} NUScClient;
u32 osSetIntMask(u32 mask);
void func_000194e4(NUScClient *c, NUScMsg msgType) {
    u32 mask;
    mask = osSetIntMask(1);
    c->msgType = msgType;
    osSetIntMask(mask);
}
