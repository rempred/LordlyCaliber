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
    u64 pad[0x1B0 / 8];
} OSThread;
typedef s16 NUScMsg;
typedef struct NUScClient_s {
               struct NUScClient_s *next;
               OSMesgQueue *msgQ;
               NUScMsg msgType;
} NUScClient;
typedef struct {
               u16 type;
               u8 pri;
               u8 status;
               OSMesgQueue *retQueue;
} OSIoMesgHdr;
typedef struct {
               OSIoMesgHdr hdr;
               void *dramAddr;
               u32 devAddr;
               u32 size;
               void *piHandle;
} OSIoMesg;
typedef struct {
               u32 errStatus;
               void *dramAddr;
               void *C2Addr;
               u32 sectorSize;
               u32 C1ErrNum;
               u32 C1ErrSector[4];
} __OSBlockInfo;
typedef struct {
               u32 cmdType;
               u16 transferMode;
               u16 blockNum;
               s32 sectorNum;
               u32 devAddr;
               u32 bmCtlShadow;
               u32 seqCtlShadow;
               __OSBlockInfo block[2];
} __OSTranxInfo;
typedef struct OSPiHandle_s {
               struct OSPiHandle_s *next;
               u8 type;
               u8 latency;
               u8 pageSize;
               u8 relDuration;
               u8 pulse;
               u8 domain;
               u32 baseAddress;
               u32 speed;
               __OSTranxInfo transferInfo;
} OSPiHandle;
typedef struct {
               u16 type;
               u8 status;
               u8 errno;
} OSContStatus;
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
extern u32 D_800C4BD8;
extern NUCallBackList *D_800A9EA0;
extern OSMesgQueue D_800E9B88;
extern OSMesg D_800BBE60[];
extern OSThread D_800BBE80;
extern u64 D_800BE030[];
extern OSMesgQueue D_800E7988;
extern OSMesg D_800BE048[];
extern OSPiHandle *D_800E7A20;
extern OSPiHandle *D_800E79AC;
extern OSPiHandle nuPiSramHandleBody;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void osSetEventMesg(s32 event, OSMesgQueue *mq, OSMesg msg);
s32 osContInit(OSMesgQueue *mq, u8 *bitpattern, OSContStatus *status);
void osCreateThread(OSThread *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(OSThread *t);
void osStopThread(OSThread *t);
void osInvalDCache(void *vaddr, s32 nbytes);
void osCreatePiManager(s32 pri, OSMesgQueue *cmdQ, OSMesg *cmdBuf, s32 cmdMsgCnt);
OSPiHandle *osCartRomInit(void);
s32 osEPiStartDma(OSPiHandle *pihandle, OSIoMesg *mb, s32 direction);
s32 osEPiLinkHandle(OSPiHandle *eph);
void bzero(void *p, s32 n);
void func_80089054(NUScClient *c, OSMesgQueue *mq, NUScMsg msgType);
void func_800890E4(NUScClient *c, NUScMsg msgType);
void func_80089D9C(void *arg);
void func_80089C50(void);
u8 func_80089C60(void);
s32 func_80089CF8(NUScMsg mesg, void *dataPtr);
void func_80089D5C(void) {
    func_80089CF8(0x7F00, ((void *)0));
}
void func_80089D7C(void);
void func_80089D9C(void *arg);
void func_80089F40(void);
void PiDmaRead(u32 rom_addr, void *buf_ptr, u32 size);
void func_8008A040(void);
