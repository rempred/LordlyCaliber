typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef float f32;
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
typedef struct {
    u8 pad00[0x14];
    s16 *state;
    f32 ratio;
    s32 upitch;
    f32 delta;
    s32 first;
    u8 pad28[0xC];
} ALResampler;
typedef struct {
    u32 input;
    u32 output;
    s16 ffcoef;
    s16 fbcoef;
    s16 gain;
    f32 rsinc;
    f32 rsval;
    s32 rsdelta;
    f32 rsgain;
    ALLowPass *lp;
    ALResampler *rs;
} ALDelay;
typedef struct {
    u8 filter[0x14];
    s16 *base;
    s16 *input;
    u32 length;
    ALDelay *delay;
    u8 section_count;
    void *paramHdl;
} ALFx;
u32 osVirtualToPhysical(void *addr);
Acmd *func_000180a0(ALFx *r, s16 *curr_ptr, s32 buff, s32 count, Acmd *p);
Acmd *func_000180a0(ALFx *r, s16 *curr_ptr, s32 buff, s32 count, Acmd *p) {
    Acmd *ptr = p;
    s32 after_end, before_end;
    s16 *updated_ptr, *delay_end;
    delay_end = &r->base[r->length];
    if (curr_ptr < r->base) {
        curr_ptr += r->length;
    }
    updated_ptr = curr_ptr + count;
    if (updated_ptr > delay_end) {
        after_end = updated_ptr - delay_end;
        before_end = delay_end - curr_ptr;
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(4) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(before_end << 1) & ((0x01 << (12)) - 1)) << (12))) | ((u32)(((u32)(buff) & ((0x01 << (12)) - 1)) << (0))); _a->w1 = (u32)(osVirtualToPhysical(curr_ptr)); };
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(4) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(after_end << 1) & ((0x01 << (12)) - 1)) << (12))) | ((u32)(((u32)(buff + (before_end << 1)) & ((0x01 << (12)) - 1)) << (0))); _a->w1 = (u32)(osVirtualToPhysical(r->base)); };
    } else {
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(4) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(count << 1) & ((0x01 << (12)) - 1)) << (12))) | ((u32)(((u32)(buff) & ((0x01 << (12)) - 1)) << (0))); _a->w1 = (u32)(osVirtualToPhysical(curr_ptr)); };
    }
    return ptr;
}
