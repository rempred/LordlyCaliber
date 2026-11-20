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
Acmd *func_00016100(s32 sampleOffset, Acmd *p);
Acmd *func_00017e90(ALFx *r, ALDelay *d, s32 buff, Acmd *p);
Acmd *func_000180a0(ALFx *r, s16 *curr_ptr, s32 buff, s32 count, Acmd *p);
Acmd *func_000181c8(ALFx *r, s16 *curr_ptr, s32 buff, Acmd *p);
Acmd *func_000182e0(ALLowPass *lp, s32 buff, Acmd *p);
Acmd *func_00017990(s32 sampleOffset, Acmd *p) {
    Acmd *ptr = p;
    ALFx *r = D_800A9E54->auxBus->fx;
    s16 i, buff1, buff2, input, output;
    s16 *in_ptr, *out_ptr, *prev_out_ptr = 0;
    ALDelay *d;
    ptr = func_00016100(sampleOffset, p);
    input = 0x7C0;
    output = 0x930;
    buff1 = 0;
    buff2 = 0x170;
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(12) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(0) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)(0xDA83) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(0x7C0) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(input) & ((0x01 << (16)) - 1)) << (0))); };
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(12) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(0) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)(0x5A82) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(0x930) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(input) & ((0x01 << (16)) - 1)) << (0))); };
    ptr = func_000181c8(r, r->input, input, ptr);
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(2) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(output) & ((0x01 << (24)) - 1)) << (0))); _a->w1 = (u32)(184 << 1); };
    for (i = 0; i < r->section_count; i++) {
        d = &r->delay[i];
        in_ptr = &r->input[-d->input];
        out_ptr = &r->input[-d->output];
        if (in_ptr == prev_out_ptr) {
            { s16 t = buff2; buff2 = buff1; buff1 = t; };
        } else {
            ptr = func_000180a0(r, in_ptr, buff1, 184, ptr);
        }
        ptr = func_00017e90(r, d, buff2, ptr);
        if (d->ffcoef) {
            { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(12) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(0) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)((u16)d->ffcoef) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(buff1) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(buff2) & ((0x01 << (16)) - 1)) << (0))); };
            if (!d->rs && !d->lp) {
                ptr = func_000181c8(r, out_ptr, buff2, ptr);
            }
        }
        if (d->fbcoef) {
            { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(12) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(0) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)((u16)d->fbcoef) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(buff2) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(buff1) & ((0x01 << (16)) - 1)) << (0))); };
            ptr = func_000181c8(r, in_ptr, buff1, ptr);
        }
        if (d->lp) {
            ptr = func_000182e0(d->lp, buff2, ptr);
        }
        if (!d->rs) {
            ptr = func_000181c8(r, out_ptr, buff2, ptr);
        }
        if (d->gain) {
            { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(12) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(0) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)((u16)d->gain) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(buff2) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(output) & ((0x01 << (16)) - 1)) << (0))); };
        }
        prev_out_ptr = &r->input[d->output];
    }
    r->input += 184;
    if (r->input > &r->base[r->length]) {
        r->input -= r->length;
    }
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(10) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(output) & ((0x01 << (24)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(0x7C0) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(184 << 1) & ((0x01 << (16)) - 1)) << (0))); };
    return ptr;
}
Acmd *func_000180a0(ALFx *r, s16 *curr_ptr, s32 buff, s32 count, Acmd *p);
Acmd *func_000181c8(ALFx *r, s16 *curr_ptr, s32 buff, Acmd *p);
Acmd *func_000182e0(ALLowPass *lp, s32 buff, Acmd *p);
