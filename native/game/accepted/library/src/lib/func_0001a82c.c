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
    u16 type;
    u8 status;
    u8 errno;
} OSContStatus;
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
typedef s16 NUScMsg;
typedef struct {
    NUScMsg mesg;
    OSMesgQueue *rtnMesgQ;
    s32 error;
    void *dataPtr;
} NUSiCommonMesg;
extern OSMesgQueue D_800E9B88;
extern OSContStatus D_800C4B38[4];
s32 func_000205f0(OSMesgQueue *mq);
void func_00020670(OSContStatus *status);
s32 func_0001a82c(NUSiCommonMesg *mesg) {
    s32 rtn;
    rtn = func_000205f0(&D_800E9B88);
    if (rtn) {
        return rtn;
    }
    func_00023AE0(&D_800E9B88, ((void *)0), 1);
    func_00020670(D_800C4B38);
    return rtn;
}
