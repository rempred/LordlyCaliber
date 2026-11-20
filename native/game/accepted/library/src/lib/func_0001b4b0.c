typedef signed char s8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned long long u64;
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
void osCreateThread(OSThread *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(OSThread *t);
extern OSThread D_800BE1C0;
extern u64 D_800C2370[];
void func_0001b3e0(void *arg);
void func_0001b4b0(void) {
    osCreateThread(&D_800BE1C0, 4, func_0001b3e0, ((void *)0), D_800C2370, 0x32);
    osStartThread(&D_800BE1C0);
}
