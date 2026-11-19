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
typedef struct {
               u32 flags;
               u8 unk04[0x6C];
               s32 unk70;
} MapUnit;
typedef struct { u16 v[3]; } U16x3;
extern s32 D_801FDA10_ovl_145210;
extern s32 *D_801FDA14_ovl_145210;
extern s32 D_801FDA18_ovl_145210;
extern s32 D_801FDA1C_ovl_145210;
extern s32 D_801FDA20_ovl_145210;
extern u8 D_801FDA24_ovl_145210;
extern u8 D_801FDA30_ovl_145210;
extern u8 *D_801FDA3C_ovl_145210;
extern u16 D_801FDA60_ovl_145210;
extern U16x3 D_801FD958_ovl_145210;
extern s32 D_801F367C;
extern MapUnit *D_801F0CB0[];
void bzero(void *p, s32 size);
void *HMalloc(s32 size);
void HFree(void *p);
void func_801F4358_ovl_145210(s32 kind, void *msg);
typedef struct {
    s32 a;
    u8 b;
    u8 c;
    u8 pad[2];
} Msg8;
typedef struct {
               s32 id;
               u16 flags;
               u8 unk06[5];
               u8 bB;
} MapEntry;
typedef struct MapObj {
    void (*fn)(struct MapObj *self);
} MapObj;
extern u16 D_801FDA32_ovl_145210;
extern MapEntry **D_801FDA34_ovl_145210;
extern Gfx *D_800E9BA0;
extern Gfx D_801FCF70_ovl_145210[];
extern Gfx D_801FCF98_ovl_145210[];
typedef struct {
               MapUnit *u;
               u16 flags;
               u8 b6;
               u8 b7;
               u8 unk08[0x2A];
               u8 b32;
               u8 b33;
} MapMarker;
u8 func_801F4ED8_ovl_145210(MapUnit *u);
void func_801F95F8_ovl_145210(s32 a, u8 *o);
void func_801F6DC8_ovl_145210(void);
extern u8 D_801FCFC0_ovl_145210[][2];
typedef struct {
    u8 next;
    u8 used;
    u8 unk02[0x12];
} Slot145;
extern s16 D_801FDA00_ovl_145210;
extern u8 D_801FDA02_ovl_145210;
extern Slot145 *D_801FDA54_ovl_145210;
extern u8 D_801FDA31_ovl_145210;
extern s32 D_801FDA38_ovl_145210;
extern s16 D_801FDA58_ovl_145210;
extern void *D_801FDA5C_ovl_145210;
extern void *D_801FDA50_ovl_145210;
extern s16 D_801FDA68_ovl_145210;
extern s16 D_801FDA6A_ovl_145210;
void func_801F988C_ovl_145210(void);
typedef struct {
    u8 b0;
    u8 type;
    u8 unk02[2];
    u8 key[8];
    u8 unk0C[4];
    s32 value;
} Ent145;
void func_801F4578_ovl_145210(s32 a, u8 b);
void func_801F45D0_ovl_145210(s32 a, u8 b, u8 c);
extern inline s32 Cmp8_145(u8 *x, u8 *y) {
    s32 n;
    for (n = 8; n != 0; n--) {
        if (*x++ != *y++) {
            return 1;
        }
    }
    return 0;
}
void func_801F4734_ovl_145210(s16 a);
void func_801F477C_ovl_145210(s16 a);
void func_801F47C4_ovl_145210(u8 a);
void func_801F4960_ovl_145210(void);
void func_801F4A34_ovl_145210(u8 *src, u8 *dst, s32 n);
void func_801F4A6C_ovl_145210(u8 *src, u8 *img, u16 alt, u16 *dst, s32 n);
void func_801F4AD0_ovl_145210(void);
void func_801F4B14_ovl_145210(void);
void func_801F4B38_ovl_145210(void);
void func_801F4B5C_ovl_145210(void);
void func_801F4BBC_ovl_145210(s32 v);
void func_801F4BE8_ovl_145210(s32 *p, s32 v);
void func_801F4BF0_ovl_145210(s32 *p, s32 v);
void func_801F4BF8_ovl_145210(s32 v);
s32 func_801F4C04_ovl_145210(s32 v);
void func_801F4C18_ovl_145210(s32 v);
s32 func_801F4C24_ovl_145210(s32 v);
void func_801F4C38_ovl_145210(s32 on);
s32 func_801F4EB4_ovl_145210(MapUnit *u);
s32 func_801F4FF4_ovl_145210(MapUnit *u);
void func_801F503C_ovl_145210(u8 *a, u8 *b, u8 *t);
void func_801F5504_ovl_145210(u32 i);
void func_801F5C74_ovl_145210(u8 *o);
extern inline MapEntry *func_801F69A0_ovl_145210(s32 id) {
    s32 pad;
    s32 i;
    MapEntry *r = ((void *)0);
    for (i = 0; i < D_801FDA32_ovl_145210; i++) {
        if (D_801FDA34_ovl_145210[i] != ((void *)0) && D_801FDA34_ovl_145210[i]->id == id) {
            r = D_801FDA34_ovl_145210[i];
            break;
        }
    }
    return r;
}
u8 func_801F6B64_ovl_145210(s32 id);
s32 func_801F6BC0_ovl_145210(s32 id);
void func_801F6C48_ovl_145210(s32 id);
void func_801F6CB4_ovl_145210(s32 id);
s32 func_801F6D3C_ovl_145210(s32 id) {
    s32 pad;
    s32 i;
    for (i = 0; i < D_801FDA32_ovl_145210; i++) {
        if (D_801FDA34_ovl_145210[i] != ((void *)0) && D_801FDA34_ovl_145210[i]->id == id) {
            break;
        }
    }
    if (i >= D_801FDA32_ovl_145210) {
        for (;;) {
        }
    }
    return (*(u32 *)&D_801FDA34_ovl_145210[i]->flags & 0xC000000) != 0;
}
void func_801F6DC8_ovl_145210(void);
void func_801F6DD8_ovl_145210(s32 unused, s32 i);
void func_801F8510_ovl_145210(void);
void func_801F85F0_ovl_145210(void);
void func_801F85F8_ovl_145210(u8 *o);
void func_801F86A8_ovl_145210(u8 *o);
void func_801F8774_ovl_145210(u8 *o);
void func_801F8830_ovl_145210(MapMarker *m);
