typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
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
extern N_ALSynth *D_800A9E54;
Acmd *func_000166c0(void *filter, s32 sampleOffset, Acmd *p);
Acmd *func_00016100(s32 sampleOffset, Acmd *p) {
    Acmd *ptr = p;
    N_ALAuxBus *m = D_800A9E54->auxBus;
    void **sources = m->sources;
    s32 i;
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = (2 << 24) | ((0x7C0) & 0xFFFFFF); _a->w1 = (u32)(0x2E0); };
    for (i = 0; i < m->sourceCount; i++) {
        ptr = func_000166c0(sources[i], sampleOffset, ptr);
    }
    return ptr;
}
