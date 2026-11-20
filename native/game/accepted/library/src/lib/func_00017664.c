typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
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
Acmd *func_00017664(Acmd *ptr, N_PVoice *f, s32 tsam, s32 nbytes, s16 outp, s16 inp, u32 flags);
Acmd *func_00017664(Acmd *ptr, N_PVoice *f, s32 tsam, s32 nbytes, s16 outp, s16 inp, u32 flags) {
    s32 dramAlign, dramLoc;
    if (nbytes > 0) {
        dramLoc = (f->dc_dma)(f->dc_memin, nbytes, f->dc_dmaState);
        dramAlign = dramLoc & 0x7;
        nbytes += dramAlign;
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(4) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(nbytes + 8 - (nbytes & 0x7)) & ((0x01 << (12)) - 1)) << (12))) | ((u32)(((u32)(inp) & ((0x01 << (12)) - 1)) << (0))); _a->w1 = (u32)(dramLoc - dramAlign); };
    } else {
        dramAlign = 0;
    }
    if (flags & 2) {
        { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(15) & ((0x01 << (8)) - 1)) << (24))); _a->w1 = (u32)(((u32)(f->dc_lstate) & 0x1FFFFFFF)); };
    }
    { Acmd *_a = (Acmd *)ptr++; _a->w0 = ((u32)(((u32)(1) & ((0x01 << (8)) - 1)) << (24))) | ((u32)(((u32)(f->dc_state) & ((0x01 << (24)) - 1)) << (0))); _a->w1 = ((u32)(((u32)(flags) & ((0x01 << (4)) - 1)) << (28))) | ((u32)(((u32)(tsam << 1) & ((0x01 << (12)) - 1)) << (16))) | ((u32)(((u32)(dramAlign) & ((0x01 << (4)) - 1)) << (12))) | ((u32)(((u32)(outp) & ((0x01 << (12)) - 1)) << (0))); };
    f->dc_first = 0;
    return ptr;
}
