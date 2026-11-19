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
               u32 start;
               u32 end;
               u32 count;
               s16 state[16];
} ALADPCMloop;
typedef struct {
               s32 order;
               s32 npredictors;
               s16 book[1];
} ALADPCMBook;
typedef struct {
               u32 start;
               u32 end;
               u32 count;
} ALRawLoop;
typedef struct {
               u8 *base;
               s32 len;
               u8 type;
               u8 flags;
               union {
        struct {
            ALADPCMloop *loop;
            ALADPCMBook *book;
        } adpcmWave;
        struct {
            ALRawLoop *loop;
        } rawWave;
    } waveInfo;
} ALWaveTable;
typedef struct ALLink_s {
    struct ALLink_s *next;
    struct ALLink_s *prev;
} ALLink;
typedef s32 (*ALDMAproc)(s32 addr, s32 len, void *state);
typedef ALDMAproc (*ALDMANew)(void *state);
typedef struct {
               ALLink node;
               void *vvoice;
               s16 *dc_state;
               s16 *dc_lstate;
               ALRawLoop dc_loop;
               ALWaveTable *dc_table;
               s32 dc_bookSize;
               ALDMAproc dc_dma;
               void *dc_dmaState;
               s32 dc_sample;
               s32 dc_lastsam;
               s32 dc_first;
               s32 dc_memin;
               s16 *rs_state;
               f32 rs_ratio;
               s32 rs_upitch;
               f32 rs_delta;
               s32 rs_first;
               s16 *em_state;
               s16 em_pan;
               s16 em_volume;
               s16 em_cvolL;
               s16 em_cvolR;
               s16 em_dryamt;
               s16 em_wetamt;
               u16 em_lratl;
               s16 em_lratm;
               s16 em_ltgt;
               u16 em_rratl;
               s16 em_rratm;
               s16 em_rtgt;
               s32 em_delta;
               s32 em_segEnd;
               s32 em_first;
               struct ALParam_s *em_ctrlList;
               struct ALParam_s *em_ctrlTail;
               s32 em_motion;
               s32 offset;
} N_PVoice;
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
typedef struct {
               u8 pad00[0x34];
               struct {
        u8 pad00[0x14];
        s32 sourceCount;
        s32 maxSources;
        N_PVoice **sources;
        ALFx *fx;
        ALFx *fxArr[1];
    } *auxBus;
               s32 numPVoices;
               u8 pad3C[4];
               s32 outputRate;
} N_ALSynth;
extern N_ALSynth *alGlobals_800A9E54;
extern void *n_alGlobals;
typedef struct ALParam_s {
               struct ALParam_s *next;
               s32 delta;
               s16 type;
               s16 unity;
               union {
        f32 f;
        s32 i;
    } data;
               union {
        f32 f;
        s32 i;
    } moredata;
               union {
        f32 f;
        s32 i;
    } stillmoredata;
               union {
        f32 f;
        s32 i;
    } yetstillmoredata;
} ALParam;
typedef struct {
               ALLink node;
               N_PVoice *pvoice;
               ALWaveTable *table;
               void *clientPrivate;
               s16 state;
               s16 priority;
               s16 fxBus;
               s16 unityPitch;
} N_ALVoice;
ALParam *__allocParam_800889A8(void);
s32 func_80088AD4(s32 t);
s32 n_alEnvmixerParam(void *filter, s32 paramID, void *param);
void *alHeapDBAlloc(u8 *file, s32 line, void *hp, s32 num, s32 size);
void alCopy(void *src, void *dest, s32 len);
u32 osVirtualToPhysical(void *addr);
void init_lpfilter(ALLowPass *lp);
Acmd *func_80085D00(s32 sampleOffset, Acmd *p);
f32 _doModFunc(ALDelay *d, s32 count);
Acmd *_n_loadOutputBuffer(ALFx *r, ALDelay *d, s32 buff, Acmd *p);
Acmd *_n_loadBuffer(ALFx *r, s16 *curr_ptr, s32 buff, s32 count, Acmd *p);
Acmd *_n_saveBuffer(ALFx *r, s16 *curr_ptr, s32 buff, Acmd *p);
Acmd *_n_filterBuffer(ALLowPass *lp, s32 buff, Acmd *p);
Acmd *func_80087590(s32 sampleOffset, Acmd *p);
s32 func_800878A0(void *filter, s32 paramID, void *param) {
    ALFx *f = (ALFx *)filter;
    s32 p = (paramID - 2) % 8;
    s32 s = (paramID - 2) / 8;
    s32 val = *(s32 *)param;
    switch (p) {
        case 0:
            f->delay[s].input = (u32)val & 0xFFFFFFF8;
            break;
        case 1:
            f->delay[s].output = (u32)val & 0xFFFFFFF8;
            break;
        case 3:
            f->delay[s].ffcoef = (s16)val;
            break;
        case 2:
            f->delay[s].fbcoef = (s16)val;
            break;
        case 4:
            f->delay[s].gain = (s16)val;
            break;
        case 5:
            f->delay[s].rsinc = ((((f32)val) / 1000) * 2.0) / alGlobals_800A9E54->outputRate;
            break;
        case 6:
            f->delay[s].rsgain = (((f32)val) / 173123.404906676) * (f->delay[s].output - f->delay[s].input);
            break;
        case 7:
            if (f->delay[s].lp) {
                f->delay[s].lp->fc = (s16)val;
                init_lpfilter(f->delay[s].lp);
            }
            break;
    }
    return 0;
}
Acmd *_n_loadBuffer(ALFx *r, s16 *curr_ptr, s32 buff, s32 count, Acmd *p);
Acmd *_n_saveBuffer(ALFx *r, s16 *curr_ptr, s32 buff, Acmd *p);
Acmd *_n_filterBuffer(ALLowPass *lp, s32 buff, Acmd *p);
