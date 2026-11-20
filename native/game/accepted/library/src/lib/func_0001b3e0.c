typedef signed char s8;
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
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
typedef s16 NUScMsg;
typedef struct NUScClient_s {
    struct NUScClient_s *next;
    OSMesgQueue *msgQ;
    NUScMsg msgType;
} NUScClient;
extern OSMesgQueue D_800C4C28;
extern OSMesg D_800BE1A0[];
extern void (*D_800AA090)(u32);
extern void (*D_800AA094)(void);
extern u32 D_800E79A4;
void func_00019454(NUScClient *c, OSMesgQueue *mq, NUScMsg msgType);
void func_0001b3e0(void *arg) {
    NUScClient gfxClient;
    NUScMsg *mesg_type;
    osCreateMesgQueue(&D_800C4C28, D_800BE1A0, 8);
    func_00019454(&gfxClient, &D_800C4C28, 3);
    while (1) {
        func_00023AE0(&D_800C4C28, (OSMesg *)&mesg_type, 1);
        switch (*mesg_type) {
            case 1:
                if (D_800AA090 != ((void *)0)) {
                    (*D_800AA090)(D_800E79A4);
                }
                break;
            case 2:
                if (D_800AA094 != ((void *)0)) {
                    (*D_800AA094)();
                }
                break;
            default:
                break;
        }
    }
}
