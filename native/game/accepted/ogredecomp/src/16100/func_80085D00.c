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
               u8 filter[0x14];
               s32 sourceCount;
               s32 maxSources;
               void **sources;
} N_ALAuxBus;
typedef struct {
               u8 pad00[0x34];
               N_ALAuxBus *auxBus;
} N_ALSynth;
extern N_ALSynth *alGlobals_800A9E54;
Acmd *n_alEnvmixerPull(void *filter, s32 sampleOffset, Acmd *p);
Acmd *func_80085D00(s32 sampleOffset, Acmd *p) {
    Acmd *ptr = p;
    N_ALAuxBus *m = alGlobals_800A9E54->auxBus;
    void **sources = m->sources;
    s32 i;
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = (2 << 24) | ((0x7C0) & 0xFFFFFF); _a->w1 = (u32)(0x2E0); };
    for (i = 0; i < m->sourceCount; i++) {
        ptr = n_alEnvmixerPull(sources[i], sampleOffset, ptr);
    }
    return ptr;
}
