typedef signed short s16;
typedef signed int s32;
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
typedef struct {
    NUScMsg mesg;
    OSMesgQueue *rtnMesgQ;
    s32 error;
    void *dataPtr;
} NUSiCommonMesg;
extern OSMesgQueue D_800E7988;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 func_00023C10(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
s32 func_0001a0f8(NUScMsg mesg, void *dataPtr) {
    OSMesgQueue rtnMesgQ;
    NUSiCommonMesg siCommonMesg;
    OSMesg rtnMesgBuf;
    siCommonMesg.mesg = mesg;
    siCommonMesg.dataPtr = dataPtr;
    siCommonMesg.rtnMesgQ = &rtnMesgQ;
    osCreateMesgQueue(&rtnMesgQ, &rtnMesgBuf, 1);
    func_00023C10(&D_800E7988, (OSMesg)&siCommonMesg, 1);
    func_00023AE0(&rtnMesgQ, ((void *)0), 1);
    return siCommonMesg.error;
}
