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
               struct OSThread_s *mtqueue;
               struct OSThread_s *fullqueue;
               s32 validCount;
               s32 first;
               s32 msgCount;
               OSMesg *msg;
} OSMesgQueue;
typedef struct {
                u64 at, v0, v1, a0, a1, a2, a3;
                u64 t0, t1, t2, t3, t4, t5, t6, t7;
                u64 s0, s1, s2, s3, s4, s5, s6, s7;
                u64 t8, t9, gp, sp, s8, ra;
                u64 lo, hi;
                u32 sr, pc, cause, badvaddr, rcp;
                u32 fpcsr;
                u64 fp[16];
} __OSThreadContext;
typedef struct OSThread_s {
               struct OSThread_s *next;
               s32 priority;
               struct OSThread_s **queue;
               struct OSThread_s *tlnext;
               u16 state;
               u16 flags;
               s32 id;
               s32 fp;
               __OSThreadContext context;
} OSThread;
typedef struct {
               u32 w0;
               u32 w1;
} Gfx;
typedef struct {
               u16 type;
               u8 status;
               u8 errno;
} OSContStatus;
typedef struct {
               u16 button;
               s8 stick_x;
               s8 stick_y;
               u8 errno;
} OSContPad;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void osCreateThread(OSThread *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(OSThread *t);
void osStopThread(OSThread *t);
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
               struct OSPiHandle_s *piHandle;
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
               s32 active;
               OSThread *thread;
               OSMesgQueue *cmdQueue;
               OSMesgQueue *evtQueue;
               OSMesgQueue *acsQueue;
               s32 (*dma)(s32, u32, void *, u32);
               s32 (*edma)(OSPiHandle *, s32, u32, void *, u32);
} OSDevMgr;
typedef u64 OSTime;
typedef struct OSTimer_s {
               struct OSTimer_s *next;
               struct OSTimer_s *prev;
               OSTime interval;
               OSTime value;
               OSMesgQueue *mq;
               OSMesg msg;
} OSTimer;
typedef struct {
               u32 ramarray[15];
               u32 pifstatus;
} OSPifRam;
typedef struct {
               u8 dummy;
               u8 txsize;
               u8 rxsize;
               u8 cmd;
               u16 button;
               s8 stick_x;
               s8 stick_y;
} __OSContReadFormat;
typedef struct {
               u8 dummy;
               u8 txsize;
               u8 rxsize;
               u8 cmd;
               u8 typeh;
               u8 typel;
               u8 status;
               u8 dummy1;
} __OSContRequesFormat;
extern OSPifRam __osContPifRam_800C4A20;
extern u8 __osContLastCmd;
extern u8 __osMaxControllers;
s32 __osSiRawStartDma(s32 direction, void *dramAddr);
void __osSiGetAccess(void);
void __osSiRelAccess(void);
void __osSiCreateAccessQueue(void);
OSTime osGetTime(void);
s32 osSetTimer(OSTimer *t, OSTime value, OSTime interval, OSMesgQueue *mq, OSMesg msg);
typedef struct {
               u32 ctrl;
               u32 width;
               u32 burst;
               u32 vSync;
               u32 hSync;
               u32 leap;
               u32 hStart;
               u32 xScale;
               u32 vCurrent;
} OSViCommonRegs;
typedef struct {
               u32 origin;
               u32 yScale;
               u32 vStart;
               u32 vBurst;
               u32 vIntr;
} OSViFieldRegs;
typedef struct {
               u8 type;
               OSViCommonRegs comRegs;
               OSViFieldRegs fldRegs[2];
} OSViMode;
typedef struct {
               f32 factor;
               u16 offset;
               u32 scale;
} __OSViScale;
typedef struct {
               u16 state;
               u16 retraceCount;
               void *framep;
               OSViMode *modep;
               u32 control;
               OSMesgQueue *msgq;
               OSMesg msg;
               __OSViScale x;
               __OSViScale y;
} __OSViContext;
extern __OSViContext *__osViCurr;
extern __OSViContext *__osViNext;
typedef struct {
               s32 status;
               OSMesgQueue *queue;
               s32 channel;
               u8 id[32];
               u8 label[32];
               s32 version;
               s32 dir_size;
               s32 inode_table;
               s32 minode_table;
               s32 dir_table;
               s32 inode_start_page;
               u8 banks;
               u8 activebank;
} OSPfs;
typedef struct {
               u8 dummy;
               u8 txsize;
               u8 rxsize;
               u8 cmd;
               u8 addrh;
               u8 addrl;
               u8 data[32];
               u8 datacrc;
} __OSContRamReadFormat;
typedef struct {
               u8 txsize;
               u8 rxsize;
               u8 cmd;
               u8 typeh;
               u8 typel;
               u8 status;
} __OSContRequesFormatShort;
extern OSPifRam __osPfsPifRam_800E86C0;
extern u8 __osPfsInodeCacheBank;
s32 __osContRamRead(OSMesgQueue *mq, int channel, u16 address, u8 *buffer);
s32 __osContRamWrite(OSMesgQueue *mq, int channel, u16 address, u8 *buffer, int force);
s32 __osPfsGetStatus(OSMesgQueue *queue, int channel);
s32 __osPfsSelectBank(OSPfs *pfs, u8 bank);
u8 __osContAddressCrc(u16 addr);
u8 __osContDataCrc(u8 *data);
typedef union {
    struct {
        u8 bank;
        u8 page;
    } inode_t;
    u16 ipage;
} __OSInodeUnit;
typedef struct {
    __OSInodeUnit inode_page[128];
} __OSInode;
typedef struct {
               u32 game_code;
               u16 company_code;
               __OSInodeUnit start_page;
               u8 status;
               s8 reserved;
               u16 data_sum;
               u8 ext_name[4];
               u8 game_name[16];
} __OSDir;
typedef struct {
               u32 file_size;
               u32 game_code;
               u16 company_code;
               signed char ext_name[4];
               signed char game_name[16];
} OSPfsState;
s32 __osCheckId(OSPfs *pfs);
s32 __osGetId(OSPfs *pfs);
s32 __osPfsRWInode(OSPfs *pfs, __OSInode *inode, u8 flag, u8 bank);
u32 __osDisableInt(void);
void __osRestoreInt(u32 mask);
s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
OSMesgQueue *osPiGetCmdQueue(void);
u32 osVirtualToPhysical(void *addr);
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
s32 func_80089CF8(NUScMsg mesg, void *dataPtr);
typedef struct {
               OSPfs *pfs;
               s32 file_no;
               s32 error;
               u32 type;
} NUContPakFile;
typedef struct {
               NUContPakFile *file;
               s32 size;
} NUContPakFreeMesg;
typedef struct {
               NUContPakFile *file;
               u8 *noteName;
               u8 *extName;
               s32 size;
               s32 mode;
} NUContPakFileOpenMesg;
typedef struct {
               NUContPakFile *file;
               s32 offset;
               s32 size;
               u8 mode;
               u8 *buf;
} NUContPakFileRWMesg;
typedef struct {
               NUContPakFile *file;
               void *state;
} NUContPakFileStateMesg;
typedef struct {
               NUContPakFile *file;
               s32 *max_files;
               s32 *used_files;
} NUContPakFileNumMesg;
extern OSMesgQueue D_800E9B88;
extern NUCallBackList D_800AA040;
extern u16 D_800AA010;
extern u32 D_800AA014;
void func_8008B130(NUCallBackList *list);
void func_8008B1D0(NUCallBackList *list);
s32 osPfsInitPak(OSMesgQueue *queue, OSPfs *pfs, s32 channel);
s32 osPfsFreeBlocks(OSPfs *pfs, s32 *bytes_not_used);
s32 osPfsReadWriteFile(OSPfs *pfs, s32 file_no, u8 flag, s32 offset, s32 size, u8 *data_buffer);
s32 osPfsFindFile(OSPfs *pfs, u16 company_code, u32 game_code, u8 *game_name, u8 *ext_name, s32 *file_no);
s32 osPfsAllocateFile(OSPfs *pfs, u16 company_code, u32 game_code, u8 *game_name, u8 *ext_name, s32 file_size_in_bytes, s32 *file_no);
s32 osPfsDeleteFile(OSPfs *pfs, u16 company_code, u32 game_code, u8 *game_name, u8 *ext_name);
s32 osPfsFileState(OSPfs *pfs, s32 file_no, void *state);
s32 osPfsNumFiles(OSPfs *pfs, s32 *max_files, s32 *files_used);
s32 osPfsRepairId(OSPfs *pfs);
void func_8008A6A0(void);
void func_8008A6C0(void);
s32 func_8008A6E0(NUSiCommonMesg *mesg);
s32 func_8008A730(NUSiCommonMesg *mesg);
s32 func_8008A758(NUSiCommonMesg *mesg);
s32 func_8008A798(NUSiCommonMesg *mesg) {
    NUContPakFileOpenMesg *openMesg;
    NUContPakFile *file;
    s32 rtn;
    openMesg = (NUContPakFileOpenMesg *)mesg->dataPtr;
    file = openMesg->file;
    rtn = osPfsFindFile(file->pfs, D_800AA010, D_800AA014, openMesg->noteName, openMesg->extName,
                        &file->file_no);
    if ((rtn == 5) && (openMesg->mode == 1)) {
        rtn = osPfsAllocateFile(file->pfs, D_800AA010, D_800AA014, openMesg->noteName, openMesg->extName,
                                openMesg->size, &file->file_no);
    }
    return rtn;
}
s32 func_8008A850(NUSiCommonMesg *mesg);
s32 func_8008A890(NUSiCommonMesg *mesg);
s32 func_8008A8BC(NUSiCommonMesg *mesg);
s32 func_8008A8E8(NUSiCommonMesg *mesg);
