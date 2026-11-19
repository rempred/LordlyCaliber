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
typedef struct {
               Acmd *data;
               s32 size;
               u64 *ucode;
               u64 *ucode_data;
} MusTask;
typedef struct {
    void (*install)(void);
    void (*waitframe)(void);
    void (*dotask)(MusTask *task);
} MusSchedCallbacks;
extern u8 D_800B18B0[];
extern u8 *D_800B1A60;
extern AudioBuffer *D_800B1A64;
extern Acmd *D_800B1A68;
extern u8 D_800E7D18[];
extern MusSchedCallbacks *D_800A9B8C;
extern AudioBuffer *D_800A9B90;
extern u64 n_aspMainTextStart[];
extern u64 D_800ABDA0[];
void *func_800851C0(s32 count, s32 length);
void *func_80085B24(void);
void func_80084970(void *g, ALSynConfig *c);
u32 func_80085BD0(u32 retrace, u32 output_rate, u32 vsyncs_per_second, u32 extra_ratio);
void *__MusIntMemMalloc(s32 size);
void func_80085334(void);
u32 func_80085C74(u32 samples);
Acmd *func_80088840(Acmd *cmdList, s32 *cmdLen, s16 *outBuf, s32 outLen);
s32 osAiSetFrequency(u32 frequency);
u32 osAiGetStatus(void);
u32 osAiGetLength(void);
s32 osAiSetNextBuffer(void *buf, u32 size);
u32 osVirtualToPhysical(void *addr);
void osCreateThread(void *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(void *t);
void func_80085908(void *arg);
void func_800857D0(MusConfig *config, s32 vsyncs_per_second, s32 fx_type) {
    ALSynConfig audio_config;
    u32 i;
    s32 frame_size;
    audio_config.maxVVoices = audio_config.maxPVoices = config->channels;
    audio_config.maxUpdates = config->syn_updates;
    audio_config.dmaproc = func_800851C0(config->syn_num_dma_bufs, config->syn_dma_buf_size);
    audio_config.fxType = fx_type;
    audio_config.outputRate = osAiSetFrequency(config->syn_output_rate);
    audio_config.heap = func_80085B24();
    func_80084970(D_800E7D18, &audio_config);
    frame_size = func_80085BD0(config->syn_retraceCount, audio_config.outputRate, vsyncs_per_second, 20);
    D_800B1A68 = __MusIntMemMalloc(config->syn_rsp_cmds * sizeof(Acmd));
    D_800B1A64 = __MusIntMemMalloc(3 * sizeof(AudioBuffer));
    for (i = 0; i < 3; i++) {
        D_800B1A64[i].ptr = __MusIntMemMalloc(frame_size * 4);
    }
    D_800B1A60 = __MusIntMemMalloc(0x2000);
    osCreateThread(D_800B18B0, 3, func_80085908, ((void *)0), D_800B1A60 + 0x2000, config->thread_priority);
    osStartThread(D_800B18B0);
}
void func_80085908(void *arg);
