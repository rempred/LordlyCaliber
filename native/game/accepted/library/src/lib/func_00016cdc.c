typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef double f64;
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
u32 osVirtualToPhysical(void *addr);
Acmd *func_000177b0(N_PVoice *e, s16 *outp, Acmd *p);
s16 func_00016efc(f64 vol, f64 tgt, s32 count, u16 *ratel);
extern s16 D_800A9D40[];
Acmd *func_00016cdc(void *filter, s16 *inp, s16 *outp, s32 outCount, Acmd *p);
Acmd *func_00016cdc(void *filter, s16 *inp, s16 *outp, s32 outCount, Acmd *p) {
    Acmd *ptr = p;
    N_PVoice *e = filter;
    if (e->em_motion != 1 || !outCount) {
        return ptr;
    }
    ptr = func_000177b0(e, inp, p);
    if (e->em_first) {
        e->em_first = 0;
        e->em_ltgt = (e->em_volume * D_800A9D40[e->em_pan]) >> 15;
        e->em_lratm = func_00016efc((f64)e->em_cvolL, (f64)e->em_ltgt, e->em_segEnd, &e->em_lratl);
        e->em_rtgt = (e->em_volume * D_800A9D40[128 - e->em_pan - 1]) >> 15;
        e->em_rratm = func_00016efc((f64)e->em_cvolR, (f64)e->em_rtgt, e->em_segEnd, &e->em_rratl);
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(9) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(0) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)(e->em_ltgt) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(e->em_lratm) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(e->em_lratl) & ((0x01 << (16)) - 1)) << (0))); };
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(9) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(6) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)(e->em_cvolL) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(e->em_dryamt) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(e->em_wetamt) & ((0x01 << (16)) - 1)) << (0))); };
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(9) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(4) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)(e->em_rtgt) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(e->em_rratm) & ((0x01 << (16)) - 1)) << (16))) | ((u32)(((u32)(e->em_rratl) & ((0x01 << (16)) - 1)) << (0))); };
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(3) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(1) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)(e->em_cvolR) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = (u32)(osVirtualToPhysical(e->em_state)); };
    } else {
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(3) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(0) & ((0x01 << (8)) - 1)) << (16))) | ((u32)(((u32)(0) & ((0x01 << (16)) - 1)) << (0))); _a->w1 = (u32)(osVirtualToPhysical(e->em_state)); };
    }
    *inp += 184 << 1;
    e->em_delta += 184;
    return ptr;
}
s16 func_00016efc(f64 vol, f64 tgt, s32 count, u16 *ratel);
