typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef float f32;
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
extern N_ALSynth *D_800A9E54;
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
void *alHeapDBAlloc(u8 *file, s32 line, void *hp, s32 num, s32 size);
typedef struct ALPlayer_s {
    struct ALPlayer_s *next;
    void *clientData;
    s32 (*handler)(void *);
    s32 callTime;
    s32 samplesLeft;
} ALPlayer;
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
typedef struct {
    ALPlayer *head;
    ALLink pFreeList;
    ALLink pAllocList;
    ALLink pLameList;
    s32 paramSamples;
    s32 curSamples;
    void *dma;
    void *heap;
    ALParam *paramList;
    struct {
        u8 pad00[4];
        void *handler;
    } *mainBus;
    struct {
        u8 pad00[0x14];
        s32 sourceCount;
        s32 maxSources;
        N_PVoice **sources;
        void *fx;
    } *auxBus;
    s32 numPVoices;
    u8 pad3C[4];
    s32 outputRate;
    s32 maxOutSamples;
    s32 sv_dramout;
    s32 sv_first;
} N_ALSynthFull;
void alLink(ALLink *ln, ALLink *to);
void func_0001658c(N_PVoice *mv, void *dmaNew, void *hp);
void *func_00018f80(s16 bus, ALSynConfig *c, void *hp);
Acmd *func_00017990(s32 sampleOffset, Acmd *p);
Acmd *func_00016100(s32 sampleOffset, Acmd *p);
void func_000189d0(ALSynConfig *c) {
    s32 i;
    N_PVoice *pv;
    N_PVoice *pvoices;
    ALParam *params;
    void *hp = c->heap;
    ((N_ALSynthFull *)D_800A9E54)->head = ((void *)0);
    ((N_ALSynthFull *)D_800A9E54)->numPVoices = c->maxPVoices;
    ((N_ALSynthFull *)D_800A9E54)->curSamples = 0;
    ((N_ALSynthFull *)D_800A9E54)->paramSamples = 0;
    ((N_ALSynthFull *)D_800A9E54)->outputRate = c->outputRate;
    ((N_ALSynthFull *)D_800A9E54)->maxOutSamples = 184;
    ((N_ALSynthFull *)D_800A9E54)->dma = c->dmaproc;
    ((N_ALSynthFull *)D_800A9E54)->sv_dramout = 0;
    ((N_ALSynthFull *)D_800A9E54)->sv_first = 1;
    ((N_ALSynthFull *)D_800A9E54)->auxBus = alHeapDBAlloc(0, 0, (hp), (1), (0x44));
    ((N_ALSynthFull *)D_800A9E54)->auxBus->sourceCount = 0;
    ((N_ALSynthFull *)D_800A9E54)->auxBus->maxSources = c->maxPVoices;
    ((N_ALSynthFull *)D_800A9E54)->auxBus->sources = alHeapDBAlloc(0, 0, (hp), (c->maxPVoices), (4));
    ((N_ALSynthFull *)D_800A9E54)->mainBus = alHeapDBAlloc(0, 0, (hp), (1), (0x14));
    if (c->fxType != 0) {
        ((N_ALSynthFull *)D_800A9E54)->auxBus->fx = func_00018f80(0, c, hp);
        ((N_ALSynthFull *)D_800A9E54)->mainBus->handler = func_00017990;
    } else {
        ((N_ALSynthFull *)D_800A9E54)->mainBus->handler = func_00016100;
    }
    ((N_ALSynthFull *)D_800A9E54)->pFreeList.next = ((void *)0);
    ((N_ALSynthFull *)D_800A9E54)->pFreeList.prev = ((void *)0);
    ((N_ALSynthFull *)D_800A9E54)->pLameList.next = ((void *)0);
    ((N_ALSynthFull *)D_800A9E54)->pLameList.prev = ((void *)0);
    ((N_ALSynthFull *)D_800A9E54)->pAllocList.next = ((void *)0);
    ((N_ALSynthFull *)D_800A9E54)->pAllocList.prev = ((void *)0);
    pvoices = alHeapDBAlloc(0, 0, (hp), (c->maxPVoices), (sizeof(N_PVoice)));
    for (i = 0; i < c->maxPVoices; i++) {
        pv = &pvoices[i];
        alLink(&pv->node, &((N_ALSynthFull *)D_800A9E54)->pFreeList);
        pv->vvoice = ((void *)0);
        func_0001658c(pv, ((N_ALSynthFull *)D_800A9E54)->dma, hp);
        ((N_ALSynthFull *)D_800A9E54)->auxBus->sources[((N_ALSynthFull *)D_800A9E54)->auxBus->sourceCount++] = pv;
    }
    params = alHeapDBAlloc(0, 0, (hp), (c->maxUpdates), (sizeof(ALParam)));
    ((N_ALSynthFull *)D_800A9E54)->paramList = ((void *)0);
    for (i = 0; i < c->maxUpdates; i++) {
        params[i].next = ((N_ALSynthFull *)D_800A9E54)->paramList;
        ((N_ALSynthFull *)D_800A9E54)->paramList = &params[i];
    }
    ((N_ALSynthFull *)D_800A9E54)->heap = hp;
}
