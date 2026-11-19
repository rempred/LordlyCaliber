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
typedef struct {
    u32 w0;
    u32 w1;
} Acmd;
typedef struct {
               u8 pad00[0x40];
               void *rs_state;
               f32 rs_ratio;
               s32 rs_upitch;
               f32 rs_delta;
               s32 rs_first;
} N_PVoice;
Acmd *n_alAdpcmPull(N_PVoice *f, s16 *outp, s32 byteCount, Acmd *p);
s32 n_alLoadParam(void *filter, s32 paramID, void *param);
u32 osVirtualToPhysical(void *addr);
Acmd *n_alResamplePull(N_PVoice *e, s16 *outp, Acmd *p) {
    Acmd *ptr = p;
    s16 inp;
    s32 inCount;
    s32 incr;
    f32 finCount;
    inp = 0x170;
    if (e->rs_upitch) {
        ptr = n_alAdpcmPull(e, &inp, 184, p);
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(10) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(inp) & ((0x01 << (24)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(*outp) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(184 << 1) & ((0x01 << (16)) - 1)) << (0))); };
    } else {
        if (e->rs_ratio > 1.99996) {
            e->rs_ratio = 1.99996;
        }
        e->rs_ratio = (s32)(e->rs_ratio * 0x8000) / (f32)0x8000;
        finCount = e->rs_delta + (184 * e->rs_ratio);
        inCount = (s32)finCount;
        e->rs_delta = finCount - (f32)inCount;
        ptr = n_alAdpcmPull(e, &inp, inCount, p);
        incr = (s32)(e->rs_ratio * 0x8000);
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(5) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(osVirtualToPhysical(e->rs_state)) & ((0x01 << (24)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(e->rs_first) & ((0x01 << (2)) - 1)) << (30))) | ((u32)(((u32)(incr) & ((0x01 << (16)) - 1)) << (14))) | ((u32)(((u32)(inp) & ((0x01 << (12)) - 1)) << (2))); };
        e->rs_first = 0;
    }
    return ptr;
}
s32 n_alResampleParam(void *filter, s32 paramID, void *param);
