typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned long long u64;
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
    u16 button;
    s8 stick_x;
    s8 stick_y;
    u8 errno;
} OSContPad;
s32 func_00023C10(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
typedef s16 NUScMsg;
typedef struct {
    NUScMsg mesg;
    OSMesgQueue *rtnMesgQ;
    s32 error;
    void *dataPtr;
} NUSiCommonMesg;
extern OSMesgQueue D_800C4A00;
extern u32 D_800C4B30;
extern OSContPad D_800C4BF0[4];
extern void (*D_800A9FE0)(u32);
s32 func_0001a7a4(OSContPad *pad, u32 lockflag);
s32 func_0001a7a4(OSContPad *pad, u32 lockflag);
s32 func_0001a87c(NUSiCommonMesg *mesg) {
    if (D_800C4B30) {
        return 0;
    }
    func_00023AE0(&D_800C4A00, ((void *)0), 0);
    func_0001a7a4(D_800C4BF0, 1);
    if (D_800A9FE0) {
        (*D_800A9FE0)(mesg->mesg);
    }
    func_00023C10(&D_800C4A00, ((void *)0), 0);
    return 0;
}
