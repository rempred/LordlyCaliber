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
void n_alLoadParam(void *filter, s32 paramID, void *param);
Acmd *n_alResamplePull(N_PVoice *e, s16 *outp, Acmd *p);
s16 _getRate_80086AFC(f64 vol, f64 tgt, s32 count, u16 *ratel);
extern s16 D_800A9D40[];
typedef struct {
               ALParam *next;
               s32 delta;
               s16 type;
               s16 unity;
               f32 pitch;
               s16 volume;
               u8 pan;
               u8 fxMix;
               s32 samples;
               ALWaveTable *wave;
} ALStartParamAlt;
typedef struct {
               ALParam *next;
               s32 delta;
               s16 type;
               N_PVoice *pvoice;
} ALFreeParam;
Acmd *_pullSubFrame_800868DC(void *filter, s16 *inp, s16 *outp, s32 outCount, Acmd *p);
s16 _getVol_80086C3C(s16 ivol, s32 samples, s16 ratem, u16 ratel);
void _n_freePVoice(N_PVoice *pvoice);
void __freeParam_800889D4(ALParam *param);
Acmd *n_alEnvmixerPull(N_PVoice *filter, s32 sampleOffset, Acmd *p) {
    Acmd *ptr = p;
    N_PVoice *e = filter;
    s16 inp;
    s32 lastOffset;
    s32 thisOffset = sampleOffset;
    s32 samples;
    s16 loutp = 0;
    s32 fVol;
    ALParam *thisParam;
    s32 outCount = 184;
    inp = 0;
    while (e->em_ctrlList != 0) {
        lastOffset = thisOffset;
        thisOffset = e->em_ctrlList->delta;
        samples = (((thisOffset - lastOffset) + 184 / 2) / 184) * 184;
        if (samples == 0) {
            thisOffset = lastOffset;
        }
        if (samples > outCount) {
            break;
        }
        switch (e->em_ctrlList->type) {
            case 13: {
                ALStartParamAlt *param = (ALStartParamAlt *)e->em_ctrlList;
                if (param->unity) {
                    e->rs_upitch = 1;
                }
                n_alLoadParam(e, 5, param->wave);
                e->em_motion = 1;
                e->em_first = 1;
                e->em_delta = 0;
                e->em_segEnd = ((param->samples + 184 / 2) / 184) * 184;
                e->em_volume = ((s32)param->volume * (s32)param->volume) >> 15;
                e->em_pan = param->pan;
                e->em_dryamt = D_800A9D40[param->fxMix];
                e->em_wetamt = D_800A9D40[128 - param->fxMix - 1];
                if (param->samples) {
                    e->em_cvolL = 1;
                    e->em_cvolR = 1;
                } else {
                    e->em_cvolL = (e->em_volume * D_800A9D40[e->em_pan]) >> 15;
                    e->em_cvolR = (e->em_volume * D_800A9D40[128 - e->em_pan - 1]) >> 15;
                }
                e->rs_ratio = param->pitch;
            } break;
            case 16:
            case 12:
            case 11:
                ptr = _pullSubFrame_800868DC(e, &inp, &loutp, samples, ptr);
                if (e->em_delta >= e->em_segEnd) {
                    e->em_ltgt = (e->em_volume * D_800A9D40[e->em_pan]) >> 15;
                    e->em_rtgt = (e->em_volume * D_800A9D40[128 - e->em_pan - 1]) >> 15;
                    e->em_delta = e->em_segEnd;
                    e->em_cvolL = e->em_ltgt;
                    e->em_cvolR = e->em_rtgt;
                } else {
                    e->em_cvolL = _getVol_80086C3C(e->em_cvolL, e->em_delta, e->em_lratm, e->em_lratl);
                    e->em_cvolR = _getVol_80086C3C(e->em_cvolR, e->em_delta, e->em_rratm, e->em_rratl);
                }
                if (e->em_cvolL == 0) {
                    e->em_cvolL = 1;
                }
                if (e->em_cvolR == 0) {
                    e->em_cvolR = 1;
                }
                if (e->em_ctrlList->type == 12) {
                    e->em_pan = (s16)e->em_ctrlList->data.i;
                }
                if (e->em_ctrlList->type == 11) {
                    e->em_delta = 0;
                    fVol = (e->em_ctrlList->data.i);
                    fVol = (fVol * fVol) >> 15;
                    e->em_volume = (s16)fVol;
                    e->em_segEnd = ((e->em_ctrlList->moredata.i + 184 / 2) / 184) * 184;
                }
                if (e->em_ctrlList->type == 16) {
                    e->em_dryamt = D_800A9D40[e->em_ctrlList->data.i];
                    e->em_wetamt = D_800A9D40[128 - e->em_ctrlList->data.i - 1];
                }
                e->em_first = 1;
                break;
            case 14: {
                ALParam *param = e->em_ctrlList;
                if (param->unity) {
                    e->rs_upitch = 1;
                }
                n_alLoadParam(e, 5, (void *)param->data.i);
                e->em_motion = 1;
            } break;
            case 15:
                ptr = _pullSubFrame_800868DC(e, &inp, &loutp, samples, ptr);
                n_alEnvmixerParam(e, 4, 0);
                break;
            case 0: {
                ALFreeParam *param = (ALFreeParam *)e->em_ctrlList;
                param->pvoice->offset = 0;
                _n_freePVoice(param->pvoice);
            } break;
            case 7:
                ptr = _pullSubFrame_800868DC(e, &inp, &loutp, samples, ptr);
                e->rs_ratio = e->em_ctrlList->data.f;
                break;
            case 8:
                ptr = _pullSubFrame_800868DC(e, &inp, &loutp, samples, ptr);
                e->rs_upitch = 1;
                break;
            case 5:
                ptr = _pullSubFrame_800868DC(e, &inp, &loutp, samples, ptr);
                n_alLoadParam(e, 5, (void *)e->em_ctrlList->data.i);
                break;
            default:
                ptr = _pullSubFrame_800868DC(e, &inp, &loutp, samples, ptr);
                n_alEnvmixerParam(e, e->em_ctrlList->type, (void *)e->em_ctrlList->data.i);
                break;
        }
        loutp += (samples << 1);
        outCount -= samples;
        thisParam = e->em_ctrlList;
        e->em_ctrlList = e->em_ctrlList->next;
        if (e->em_ctrlList == 0) {
            e->em_ctrlTail = 0;
        }
        __freeParam_800889D4(thisParam);
    }
    ptr = _pullSubFrame_800868DC(e, &inp, &loutp, outCount, ptr);
    if (e->em_delta > e->em_segEnd) {
        e->em_delta = e->em_segEnd;
    }
    return ptr;
}
s32 n_alEnvmixerParam(void *filter, s32 paramID, void *param);
Acmd *_pullSubFrame_800868DC(void *filter, s16 *inp, s16 *outp, s32 outCount, Acmd *p);
s16 _getRate_80086AFC(f64 vol, f64 tgt, s32 count, u16 *ratel);
s16 _getVol_80086C3C(s16 ivol, s32 samples, s16 ratem, u16 ratel);
