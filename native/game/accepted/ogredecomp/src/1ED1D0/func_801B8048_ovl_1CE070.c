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
typedef union {
    OSTask_t t;
    long long force_structure_alignment;
} OSTask;
typedef struct NUScTask_s {
               struct NUScTask_s *next;
               u32 state;
               u32 flags;
               void *framebuffer;
               OSTask list;
               OSMesgQueue *msgQ;
               OSMesg msg;
} NUScTask;
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
void nuGfxTaskAllEndWait(void);
s32 func_801B7B7C_ovl_1CE070(void);
extern u8 D_800E7A0C;
extern void *D_800C4BB8;
extern void *D_800A8204[3];
extern u8 D_800E7A0C;
extern void *D_800C4BB8;
extern void *D_800A8204[3];
typedef struct {
    u8 unk00[0x7E];
    u8 step;
} Seq1ED;
extern Seq1ED *D_801BA6D0_ovl_1CE070;
typedef struct {
                u8 unk00[0x84];
                u8 *data;
                u8 unk88[0x100];
                s32 size;
} BgJob;
extern NUScTask D_800E7D90[];
extern OSMesgQueue D_800E9BA8;
extern OSMesgQueue D_800E8B4C;
extern u64 D_8009ECB0[];
extern u64 D_8009ED80[];
extern u64 D_800A5110[];
extern u64 D_800A69D0[];
extern u64 D_800AD590[];
extern u64 D_800AD920[];
extern u64 D_800E8710[];
extern u64 D_800C7970[];
extern u64 D_800C6D70[];
extern u32 D_800A9E70;
extern volatile u32 D_800E79A4;
extern u8 D_801B9998_ovl_1CE070[];
void BgDecodeHu(s8 type, u16 w, u16 h, u8 *data, u8 a, u8 b);
void *HMalloc(s32 size);
void bzero(void *p, s32 n);
extern u64 D_8009F540[];
extern u64 jpegTextStart[];
extern u64 D_800AC050[];
extern u64 D_800AC140[];
void BgJpegTask(s8 mode, void *data_ptr, s16 w, s16 h);
void func_801B6E48_ovl_1CE070(void *data_ptr, u32 data_size, s32 unused, u32 flags);
void func_801B7388_ovl_1CE070(void);
void func_801B77B8_ovl_1CE070(u8 *o);
s32 BgLoad(u8 *data, u8 a, u8 b);
void func_801B8048_ovl_1CE070(void) {
    do {
        nuGfxTaskAllEndWait();
    } while (!func_801B7B7C_ovl_1CE070());
}
