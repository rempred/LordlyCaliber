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
typedef struct Node {
               struct Node *prev;
               struct Node *next;
               u8 unk08[0xE];
               u16 id;
} Node;
extern u8 D_801936E0_ovl_040E80;
extern u8 D_8018FD64_ovl_040E80[];
extern u8 D_8018FD78_ovl_040E80[];
extern u16 D_800C4C26;
extern Node *D_8018FDC0_ovl_040E80;
s32 func_8019C67C(void);
void func_8019C488(s32 a);
Node *func_8017F3C0_ovl_040E80(void);
void func_8017C2FC_ovl_040E80(Node *n);
void func_801805D0_ovl_040E80(s32 a, s32 b, s32 c, s32 d);
extern u8 D_8018FD50_ovl_040E80[];
extern u8 D_8018FD8C_ovl_040E80[];
extern u8 D_8018FDA0_ovl_040E80[];
void *func_8017B5B0_ovl_040E80(void);
u8 *func_8017B5BC_ovl_040E80(void) {
    u8 *p = D_8018FD64_ovl_040E80;
    if (D_801936E0_ovl_040E80) {
        p = D_8018FD78_ovl_040E80;
    }
    return p;
}
void *func_8017B5E0_ovl_040E80(void);
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
extern Gfx *D_800E9BA0;
extern u8 D_80187020_ovl_040E80;
extern s32 D_801977C8_ovl_040E80;
void HFree(void *p);
void func_8016C814_ovl_040E80(u8 *p);
void func_801A101C(void);
void func_801B7FA0(u8 *p);
void func_8019B040(s32 a);
extern u8 ovl_079730_TEXT_START[];
extern u8 ovl_079730_TEXT_END[];
extern u8 ovl_079730_DATA_START[];
extern u8 ovl_079730_RODATA_END[];
extern u8 ovl_079730_BSS_START[];
extern u8 ovl_079730_BSS_END[];
extern u8 ovl_079730_ROM_START[];
extern u8 ovl_079730_ROM_END[];
extern u8 ovl_079730_VRAM[];
extern u8 ovl_087200_TEXT_START[];
extern u8 ovl_087200_TEXT_END[];
extern u8 ovl_087200_DATA_START[];
extern u8 ovl_087200_RODATA_END[];
extern u8 ovl_087200_BSS_START[];
extern u8 ovl_087200_BSS_END[];
extern u8 ovl_087200_ROM_START[];
extern u8 ovl_087200_ROM_END[];
extern u8 ovl_087200_VRAM[];
extern u8 ovl_071280_TEXT_START[];
extern u8 ovl_071280_TEXT_END[];
extern u8 ovl_071280_DATA_START[];
extern u8 ovl_071280_RODATA_END[];
extern u8 ovl_071280_BSS_START[];
extern u8 ovl_071280_BSS_END[];
extern u8 ovl_071280_ROM_START[];
extern u8 ovl_071280_ROM_END[];
extern u8 ovl_071280_VRAM[];
void osInvalICache(void *p, s32 n);
void osInvalDCache(void *p, s32 n);
void RomRead(void *rom, void *dst, s32 size);
void bzero(void *p, s32 n);
void func_80073164(u8 relative, s16 a, s16 b, s16 c, s16 d, s16 e, s16 f);
void func_8019A7A0(void);
void func_801C1990(void);
void func_8019A864(void);
s32 func_8019D65C(void);
extern u16 D_800C4C26;
Node *func_8017C074_ovl_040E80(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h);
void func_801807F4_ovl_040E80(Node *n, s32 a);
void func_8017C584_ovl_040E80();
void func_8017EAE8_ovl_040E80();
void func_80221354(void);
extern u8 D_80196AEC_ovl_040E80;
void func_8017F554_ovl_040E80(Node *n);
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    u16 flag;
    s16 s;
    s16 t;
    u8 rgba[4];
} Vtx514;
void func_8017B5EC_ovl_040E80(void);
void func_8017B6B0_ovl_040E80(void);
void func_8017B774_ovl_040E80(void);
void func_8017B9A8_ovl_040E80(u8 *p);
void func_8017BA2C_ovl_040E80(void);
void *func_8017BA34_ovl_040E80(void);
void func_8017BA40_ovl_040E80(void);
void func_8017BB08_ovl_040E80(void);
void func_8017BB34_ovl_040E80(u8 *p);
void func_8017BED4_ovl_040E80(void);
Node *func_8017BFB0_ovl_040E80(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h, s16 x2, s16 y2, s16 w2, s16 h2);
void func_8017C2FC_ovl_040E80(Node *n);
void func_8017EE64_ovl_040E80(s32 a, u16 kind, s16 x, s16 y, s16 w, s16 h);
Node *func_8017F334_ovl_040E80(u16 id);
Node *func_8017F36C_ovl_040E80(u16 id);
extern inline Node *func_8017F3C0_ovl_040E80(void) {
    Node *n;
    if (D_8018FDC0_ovl_040E80 == ((void *)0)) {
        return ((void *)0);
    }
    n = D_8018FDC0_ovl_040E80->next;
    if (n != ((void *)0) && n->next != ((void *)0)) {
        do {
            n = n->next;
        } while (n->next != ((void *)0));
    }
    return n;
}
extern inline Node *tail514(void) {
    Node *n;
    if (D_8018FDC0_ovl_040E80 == ((void *)0)) {
        return ((void *)0);
    }
    n = D_8018FDC0_ovl_040E80->next;
    if (n == ((void *)0)) {
        return ((void *)0);
    }
    while (n->next != ((void *)0)) {
        n = n->next;
    }
    return n;
}
extern inline Node *func_8017F408_ovl_040E80(void) {
    Node *n;
    if (D_8018FDC0_ovl_040E80 == ((void *)0)) {
        return ((void *)0);
    }
    if (D_8018FDC0_ovl_040E80->next == ((void *)0)) {
        return ((void *)0);
    }
    n = func_8017F3C0_ovl_040E80();
    while (n != ((void *)0)) {
        if (!((*(u8 *)((u8 *)(n) + (0x14))) & 0x10)) {
            return n;
        }
        n = (n->prev != D_8018FDC0_ovl_040E80->next) ? n->prev : ((void *)0);
    }
    return n;
}
void func_801805B4_ovl_040E80(s32 a, s32 b, s32 c);
void func_80180B1C_ovl_040E80(Vtx514 *v, s16 x0, s16 y0, s16 x1, s16 y1);
s32 func_80181498_ovl_040E80(u8 *p, s32 i);
void func_801814AC_ovl_040E80(u8 *obj, s32 i, u8 *dst, u8 n);
