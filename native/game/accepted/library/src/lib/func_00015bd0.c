typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Acmd;
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
    u32 control_flag;
    s32 channels;
    void *sched;
    s32 thread_priority;
    u8 *heap;
    s32 heap_length;
    void *ptr;
    u8 *wbk;
    void *default_fxbank;
    s32 fifo_length;
    s32 syn_updates;
    s32 syn_output_rate;
    s32 syn_rsp_cmds;
    s32 syn_retraceCount;
    s32 syn_num_dma_bufs;
    s32 syn_dma_buf_size;
} MusConfig;
typedef struct {
    s16 *ptr;
    u32 len;
} AudioBuffer;
extern u8 D_800B18B0[];
extern u8 *D_800B1A60;
extern AudioBuffer *D_800B1A64;
extern Acmd *D_800B1A68;
extern u8 D_800E7D18[];
void *func_000155c0(s32 count, s32 length);
void *func_00015f24(void);
void func_00014d70(void *g, ALSynConfig *c);
u32 func_00015fd0(u32 retrace, u32 output_rate, u32 vsyncs_per_second, u32 extra_ratio);
void *func_00015edc(s32 size);
s32 func_0001c830(u32 frequency);
void osCreateThread(void *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(void *t);
void func_00015d08(void *arg);
void func_00015bd0(MusConfig *config, s32 vsyncs_per_second, s32 fx_type) {
    ALSynConfig audio_config;
    u32 i;
    s32 frame_size;
    audio_config.maxVVoices = audio_config.maxPVoices = config->channels;
    audio_config.maxUpdates = config->syn_updates;
    audio_config.dmaproc = func_000155c0(config->syn_num_dma_bufs, config->syn_dma_buf_size);
    audio_config.fxType = fx_type;
    audio_config.outputRate = func_0001c830(config->syn_output_rate);
    audio_config.heap = func_00015f24();
    func_00014d70(D_800E7D18, &audio_config);
    frame_size = func_00015fd0(config->syn_retraceCount, audio_config.outputRate, vsyncs_per_second, 20);
    D_800B1A68 = func_00015edc(config->syn_rsp_cmds * sizeof(Acmd));
    D_800B1A64 = func_00015edc(3 * sizeof(AudioBuffer));
    for (i = 0; i < 3; i++) {
        D_800B1A64[i].ptr = func_00015edc(frame_size * 4);
    }
    D_800B1A60 = func_00015edc(0x2000);
    osCreateThread(D_800B18B0, 3, func_00015d08, ((void *)0), D_800B1A60 + 0x2000, config->thread_priority);
    osStartThread(D_800B18B0);
}
void func_00015d08(void *arg);
