typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
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
typedef s16 NUScMsg;
typedef struct NUScClient_s {
    struct NUScClient_s *next;
    OSMesgQueue *msgQ;
    NUScMsg msgType;
} NUScClient;
typedef struct {
    NUScMsg mesg;
    OSMesgQueue *rtnMesgQ;
    s32 error;
    void *dataPtr;
} NUSiCommonMesg;
typedef s32 (*NUCallBackFunc)(NUSiCommonMesg *mesg);
typedef struct NUCallBackList_s {
    struct NUCallBackList_s *next;
    NUCallBackFunc *func;
    u16 majorNo;
    u8 funcNum;
} NUCallBackList;
extern NUCallBackList *D_800A9EA0;
extern OSMesgQueue D_800E7988;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 func_00023C10(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void func_00024f40(OSThread *t);
void func_00019454(NUScClient *c, OSMesgQueue *mq, NUScMsg msgType);
void func_000194e4(NUScClient *c, NUScMsg msgType);
void func_0001a19c(void *arg);
void func_0001a19c(void *arg) {
    NUScClient siClient;
    OSMesg mesgBuf[8];
    NUSiCommonMesg *siMesg;
    NUCallBackList **siCallBackListPtr;
    s32 ret;
    u16 majorNo;
    u16 minorNo;
    osCreateMesgQueue(&D_800E7988, mesgBuf, 8);
    func_00019454(&siClient, &D_800E7988, 1);
    while (1) {
        func_00023AE0(&D_800E7988, (OSMesg *)&siMesg, 1);
        siCallBackListPtr = &D_800A9EA0;
        switch (siMesg->mesg) {
            case 1:
                if (*siCallBackListPtr == ((void *)0)) {
                    continue;
                }
                while (*siCallBackListPtr) {
                    if ((*siCallBackListPtr)->func[0]) {
                        ret = (*(*siCallBackListPtr)->func[0])(siMesg);
                    }
                    if (ret) {
                        break;
                    }
                    siCallBackListPtr = &(*siCallBackListPtr)->next;
                }
                break;
            case 0x7F00:
                func_00023C10(siMesg->rtnMesgQ, ((void *)0), 1);
                func_000194e4(&siClient, 0);
                func_00024f40(((void *)0));
                func_000194e4(&siClient, 1);
                break;
            default:
                majorNo = siMesg->mesg & 0xFF00;
                minorNo = siMesg->mesg & 0x00FF;
                while (*siCallBackListPtr) {
                    if ((*siCallBackListPtr)->majorNo == majorNo) {
                        if ((*siCallBackListPtr)->funcNum > minorNo) {
                            siMesg->error = (*(*siCallBackListPtr)->func[minorNo])(siMesg);
                        }
                        if (siMesg->rtnMesgQ != ((void *)0)) {
                            func_00023C10(siMesg->rtnMesgQ, ((void *)0), 1);
                        }
                        break;
                    }
                    siCallBackListPtr = &(*siCallBackListPtr)->next;
                }
                break;
        }
    }
}
