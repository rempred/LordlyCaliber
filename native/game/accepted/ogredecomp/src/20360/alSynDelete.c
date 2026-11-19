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
typedef Acmd *(*ALCmdHandler)(void *, s16 *, s32, s32, Acmd *);
typedef s32 (*ALSetParam)(void *, s32, void *);
typedef struct ALLink_s {
               struct ALLink_s *next;
               struct ALLink_s *prev;
} ALLink;
typedef struct {
               u8 *base;
               u8 *cur;
               s32 len;
               s32 count;
} ALHeap;
typedef struct ALFilter_s {
               struct ALFilter_s *source;
               ALCmdHandler handler;
               ALSetParam setParam;
               s16 inp;
               s16 outp;
               s32 type;
} ALFilter;
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
typedef short ENVMIX_STATE[40];
typedef short RESAMPLE_STATE[16];
typedef short POLEF_STATE[4];
typedef short ADPCM_STATE[16];
typedef struct {
               s16 fc;
               s16 fgain;
               union {
        s16 fccoef[16];
        s64 force_aligned;
    } fcvec;
               POLEF_STATE *fstate;
               s32 first;
} ALLowPass;
typedef struct {
               ALFilter filter;
               RESAMPLE_STATE *state;
               f32 ratio;
               s32 upitch;
               f32 delta;
               s32 first;
               ALParam *ctrlList;
               ALParam *ctrlTail;
               s32 motion;
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
               ALFilter filter;
               ENVMIX_STATE *state;
               s16 pan;
               s16 volume;
               s16 cvolL;
               s16 cvolR;
               s16 dryamt;
               s16 wetamt;
               u16 lratl;
               s16 lratm;
               s16 ltgt;
               u16 rratl;
               s16 rratm;
               s16 rtgt;
               s32 delta;
               s32 segEnd;
               s32 first;
               ALParam *ctrlList;
               ALParam *ctrlTail;
               ALFilter **sources;
               s32 motion;
} ALEnvMixer;
typedef struct {
               u32 start;
               u32 end;
               u32 count;
} ALRawLoop;
typedef s32 (*ALDMAproc)(s32 addr, s32 len, void *state);
typedef ALDMAproc (*ALDMANew)(void *state);
typedef struct {
               ALFilter filter;
               ADPCM_STATE *state;
               ADPCM_STATE *lstate;
               ALRawLoop loop;
               struct ALWaveTable_s *table;
               s32 bookSize;
               ALDMAproc dma;
               void *dmaState;
               s32 sample;
               s32 lastsam;
               s32 first;
               s32 memin;
} ALLoadFilter;
typedef struct {
               ALFilter filter;
               s16 *base;
               s16 *input;
               u32 length;
               ALDelay *delay;
               u8 section_count;
               ALSetParam paramHdl;
} ALFx;
typedef struct {
               ALFilter filter;
               s32 sourceCount;
               s32 maxSources;
               ALFilter **sources;
               ALFx fx[1];
} ALAuxBus;
typedef struct {
               ALFilter filter;
               s32 sourceCount;
               s32 maxSources;
               ALFilter **sources;
} ALMainBus;
typedef struct {
               ALFilter filter;
               s32 dramout;
               s32 first;
} ALSave;
typedef struct ALPlayer_s {
               struct ALPlayer_s *next;
               void *clientData;
               s32 (*handler)(void *);
               s32 callTime;
               s32 samplesLeft;
} ALPlayer;
typedef struct {
               ALPlayer *head;
               ALLink pFreeList;
               ALLink pAllocList;
               ALLink pLameList;
               s32 paramSamples;
               s32 curSamples;
               ALDMANew dma;
               ALHeap *heap;
               ALParam *paramList;
               ALMainBus *mainBus;
               ALAuxBus *auxBus;
               ALFilter *outputFilter;
               s32 numPVoices;
               s32 maxAuxBusses;
               s32 outputRate;
               s32 maxOutSamples;
} ALSynth;
typedef struct {
               ALSynth drvr;
} ALGlobals;
typedef struct {
               s32 maxVVoices;
               s32 maxPVoices;
               s32 maxUpdates;
               s32 maxFXbusses;
               ALDMANew dmaproc;
               ALHeap *heap;
               s32 outputRate;
               u8 fxType;
               s32 *params;
} ALSynConfig;
typedef void *ALFxRef;
void alFilterNew(ALFilter *f, ALCmdHandler h, ALSetParam s, s32 type);
void alSynNew(ALSynth *drvr, ALSynConfig *c);
void alSynDelete(ALSynth *drvr);
void alFxNew(ALFx *r, ALSynConfig *c, ALHeap *hp);
s32 alFxParam(void *filter, s32 paramID, void *param);
s32 alMainBusParam(void *filter, s32 paramID, void *param);
void *alHeapDBAlloc(u8 *file, s32 line, ALHeap *hp, s32 num, s32 size);
void alSynDelete(ALSynth *drvr) {
    drvr->head = 0;
}
