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
void mem_byte_copy(void *src, void *dest, s32 len);
void func_000174d8(void *filter, s32 paramID, void *param) {
    N_PVoice *a = (N_PVoice *)filter;
    switch (paramID) {
        case 5:
            a->dc_table = (ALWaveTable *)param;
            a->dc_memin = (s32)a->dc_table->base;
            a->dc_sample = 0;
            switch (a->dc_table->type) {
                case 0:
                    a->dc_table->len = 9 * ((s32)(a->dc_table->len / 9));
                    a->dc_bookSize = 2 * a->dc_table->waveInfo.adpcmWave.book->order *
                                     a->dc_table->waveInfo.adpcmWave.book->npredictors * 8;
                    if (a->dc_table->waveInfo.adpcmWave.loop) {
                        a->dc_loop.start = a->dc_table->waveInfo.adpcmWave.loop->start;
                        a->dc_loop.end = a->dc_table->waveInfo.adpcmWave.loop->end;
                        a->dc_loop.count = a->dc_table->waveInfo.adpcmWave.loop->count;
                        mem_byte_copy(a->dc_table->waveInfo.adpcmWave.loop->state, a->dc_lstate, 0x20);
                    } else {
                        a->dc_loop.start = a->dc_loop.end = a->dc_loop.count = 0;
                    }
                    break;
                case 1:
                    if (a->dc_table->waveInfo.rawWave.loop) {
                        a->dc_loop.start = a->dc_table->waveInfo.rawWave.loop->start;
                        a->dc_loop.end = a->dc_table->waveInfo.rawWave.loop->end;
                        a->dc_loop.count = a->dc_table->waveInfo.rawWave.loop->count;
                    } else {
                        a->dc_loop.start = a->dc_loop.end = a->dc_loop.count = 0;
                    }
                    break;
                default:
                    break;
            }
            break;
        case 4:
            a->dc_lastsam = 0;
            a->dc_first = 1;
            a->dc_sample = 0;
            if (a->dc_table) {
                a->dc_memin = (s32)a->dc_table->base;
                if (a->dc_table->type == 0) {
                    if (a->dc_table->waveInfo.adpcmWave.loop) {
                        a->dc_loop.count = a->dc_table->waveInfo.adpcmWave.loop->count;
                    }
                } else if (a->dc_table->type == 1) {
                    if (a->dc_table->waveInfo.rawWave.loop) {
                        a->dc_loop.count = a->dc_table->waveInfo.rawWave.loop->count;
                    }
                }
            }
            break;
        default:
            break;
    }
}
