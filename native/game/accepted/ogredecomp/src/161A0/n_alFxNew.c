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
typedef struct {
               s32 maxVVoices;
               s32 maxPVoices;
               s32 maxUpdates;
               s32 maxFXbusses;
               void *dmaproc;
               void *heap;
               s32 outputRate;
               u8 fxType;
               s32 *params;
} ALSynConfig;
extern s32 D_800A9BB0[];
extern s32 D_800A9C18[];
extern s32 D_800A9CA0[];
extern s32 D_800A9CC8[];
extern s32 D_800A9CF0[];
extern s32 D_800A9D18[];
void init_lpfilter(ALLowPass *lp);
void n_alFxNew(ALFx **fx_ar, ALSynConfig *c, void *hp) {
    u16 i, j, k;
    s32 *param = 0;
    ALDelay *d;
    ALFx *r;
    *fx_ar = r = (ALFx *)alHeapDBAlloc(0, 0, (hp), (1), (sizeof(ALFx)));
    switch (c->fxType) {
        case 1:
            param = D_800A9BB0;
            break;
        case 2:
            param = D_800A9C18;
            break;
        case 5:
            param = D_800A9CA0;
            break;
        case 3:
            param = D_800A9CC8;
            break;
        case 4:
            param = D_800A9CF0;
            break;
        case 6:
            param = c->params;
            break;
        default:
            param = D_800A9D18;
            break;
    }
    j = 0;
    r->section_count = param[j++];
    r->length = param[j++];
    r->delay = alHeapDBAlloc(0, 0, (hp), (r->section_count), (sizeof(ALDelay)));
    r->base = alHeapDBAlloc(0, 0, (hp), (r->length), (sizeof(s16)));
    r->input = r->base;
    for (k = 0; k < r->length; k++) {
        r->base[k] = 0;
    }
    for (i = 0; i < r->section_count; i++) {
        d = &r->delay[i];
        d->input = param[j++];
        d->output = param[j++];
        d->fbcoef = param[j++];
        d->ffcoef = param[j++];
        d->gain = param[j++];
        if (param[j]) {
            d->rsinc = ((((f32)param[j++]) / 1000) * 2.0) / c->outputRate;
            d->rsgain = (((f32)param[j++]) / 173123.404906676) * (d->output - d->input);
            d->rsval = 1.0;
            d->rsdelta = 0.0;
            d->rs = alHeapDBAlloc(0, 0, (hp), (1), (sizeof(ALResampler)));
            d->rs->state = alHeapDBAlloc(0, 0, (hp), (1), (0x20));
            d->rs->delta = 0.0;
            d->rs->first = 1;
        } else {
            d->rs = 0;
            j++;
            j++;
        }
        if (param[j]) {
            d->lp = alHeapDBAlloc(0, 0, (hp), (1), (sizeof(ALLowPass)));
            d->lp->fstate = alHeapDBAlloc(0, 0, (hp), (1), (8));
            d->lp->fc = param[j++];
            init_lpfilter(d->lp);
        } else {
            d->lp = 0;
            j++;
        }
    }
}
void alN_PVoiceNew(N_PVoice *mv, ALDMANew dmaNew, void *hp);
