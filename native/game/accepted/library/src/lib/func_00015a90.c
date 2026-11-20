typedef signed char s8;
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
extern MusSchedData *D_800B18A4;
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void func_00015a90(void) {
    OSScMsg *msg;
    do {
        func_00023AE0(&D_800B18A4->msgQ, (OSMesg *)&msg, 1);
        func_00023AE0(&D_800B18A4->msgQ, ((void *)0), 0);
    } while (msg->type != 1);
}
