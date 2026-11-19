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
Acmd *n_alResamplePull(N_PVoice *e, s16 *outp, Acmd *p);
s32 n_alResampleParam(void *filter, s32 paramID, void *param) {
    n_alLoadParam(filter, paramID, param);
    return 0;
}
