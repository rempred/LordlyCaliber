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
extern void *D_800B18A0;
extern MusSchedData *D_800B18A4;
void *func_00015edc(s32 size);
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
void func_000241f8(void *sc, OSScClient *c, OSMesgQueue *msgQ);
void func_00015a2c(void) {
    D_800B18A4 = func_00015edc(sizeof(MusSchedData));
    osCreateMesgQueue(&D_800B18A4->msgQ, D_800B18A4->msgbuf, 4);
    osCreateMesgQueue(&D_800B18A4->taskQ, D_800B18A4->taskbuf, 4);
    func_000241f8(D_800B18A0, &D_800B18A4->client, &D_800B18A4->msgQ);
}
