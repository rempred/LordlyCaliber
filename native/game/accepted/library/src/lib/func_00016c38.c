typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
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
s32 func_00016c38(void *filter, s32 paramID, void *param);
void func_000174d8(void *filter, s32 paramID, void *param);
s32 func_00016c38(void *filter, s32 paramID, void *param) {
    N_PVoice *pv = filter;
    switch (paramID) {
        case 3:
            if (pv->em_ctrlTail) {
                pv->em_ctrlTail->next = (ALParam *)param;
            } else {
                pv->em_ctrlList = (ALParam *)param;
            }
            pv->em_ctrlTail = (ALParam *)param;
            break;
        case 4:
            pv->em_first = 1;
            pv->em_motion = 0;
            pv->em_volume = 1;
            pv->em_segEnd = 0;
            pv->rs_delta = 0.0;
            pv->rs_first = 1;
            pv->rs_upitch = 0;
            func_000174d8(pv, 4, param);
            break;
        case 9:
            pv->em_motion = 1;
            break;
        default:
            func_000174d8(pv, paramID, param);
            break;
    }
    return 0;
}
