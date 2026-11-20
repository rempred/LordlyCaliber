typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef void *OSMesg;
typedef struct {
    void *mtqueue;
    void *fullqueue;
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
} OSMesgQueue;
typedef struct {
    u64 pad[0x1B0 / 8];
} OSThread;
typedef struct {
    u32 type;
    u32 flags;
    u64 *ucode_boot;
    u32 ucode_boot_size;
    u64 *ucode;
    u32 ucode_size;
    u64 *ucode_data;
    u32 ucode_data_size;
    u64 *dram_stack;
    u32 dram_stack_size;
    u64 *output_buff;
    u64 *output_buff_size;
    u64 *data_ptr;
    u32 data_size;
    u64 *yield_data_ptr;
    u32 yield_data_size;
} OSTask;
typedef s16 NUScMsg;
typedef struct NUScTask_s {
    struct NUScTask_s *next;
    u32 state;
    u32 flags;
    void *framebuffer;
    OSTask list;
    OSMesgQueue *msgQ;
    OSMesg msg;
} NUScTask;
extern NUScTask D_800E7D90[10];
extern volatile u32 D_800E79A4;
extern NUScTask *D_800B9C80;
extern NUScMsg D_800B9C84;
extern NUScMsg D_800B9C86;
extern OSThread D_800B9C88;
extern OSMesg D_800BBE40[];
extern OSMesgQueue D_800E9BA8;
extern u64 D_8009ECB0[];
extern u64 D_8009ED80[];
extern u64 D_800E8710[];
extern u64 D_800C7970[];
extern u64 D_800C6D70[];
void osCreateThread(void *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(void *t);
void func_00019fc0(void);
void func_00019940(void *arg);
void func_00019a60(void) {
    u32 cnt;
    D_800B9C84 = 8;
    D_800B9C86 = 4;
    D_800E79A4 = 0;
    func_00019fc0();
    osCreateThread(&D_800B9C88, 5, func_00019940, ((void *)0), D_800BBE40, 60);
    osStartThread(&D_800B9C88);
    for (cnt = 0; cnt < 10; cnt++) {
        D_800E7D90[cnt].next = &D_800E7D90[cnt + 1];
        D_800E7D90[cnt].msgQ = &D_800E9BA8;
        D_800E7D90[cnt].list.type = 1;
        D_800E7D90[cnt].list.flags = 0;
        D_800E7D90[cnt].list.ucode_boot = D_8009ECB0;
        D_800E7D90[cnt].list.ucode_boot_size = (s32)D_8009ED80 - (s32)D_8009ECB0;
        D_800E7D90[cnt].list.ucode_size = 0x1000;
        D_800E7D90[cnt].list.ucode_data_size = 0x800;
        D_800E7D90[cnt].list.dram_stack = D_800E8710;
        D_800E7D90[cnt].list.dram_stack_size = 0x400;
        D_800E7D90[cnt].list.output_buff = D_800C7970;
        D_800E7D90[cnt].list.yield_data_ptr = D_800C6D70;
        D_800E7D90[cnt].list.yield_data_size = 0xC00;
        D_800E7D90[cnt].msgQ = &D_800E9BA8;
    }
    D_800E7D90[10 - 1].next = &D_800E7D90[0];
    D_800B9C80 = &D_800E7D90[0];
}
