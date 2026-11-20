typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef struct {
    u32 w0;
    u32 w1;
} Acmd;
typedef struct {
    s16 fc;
    s16 fgain;
    union {
        s16 fccoef[16];
        s64 force_aligned;
    } fcvec;
    s16 *fstate;
    s32 first;
} ALLowPass;
u32 osVirtualToPhysical(void *addr);
Acmd *func_000182e0(ALLowPass *lp, s32 buff, Acmd *p);
Acmd *func_000182e0(ALLowPass *lp, s32 buff, Acmd *p) {
    Acmd *ptr = p;
    s32 t = buff >> 8;
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(11) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(32) & ((0x01 << (24)) - 1)) << (0))); _a->w1 = (u32)(osVirtualToPhysical(lp->fcvec.fccoef)); };
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(14) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(lp->first) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)(lp->fgain) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(t) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)((u32)(osVirtualToPhysical(lp->fstate))) & ((0x01 << (24)) - 1)) << (0))); };
    lp->first = 0;
    return ptr;
}
