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
typedef struct {
               u64 *ucode;
               u64 *ucode_data;
} NUUcode;
extern NUScTask D_800E7D90[10];
extern volatile u32 D_800E79A4;
extern NUScTask *D_800B9C80;
extern NUScMsg D_800B9C84;
extern NUScMsg D_800B9C86;
extern OSThread D_800B9C88;
extern OSMesg D_800BBE40[];
extern OSMesgQueue D_800E9BA8;
extern void (*D_800A9E84)(NUScTask *);
extern void (*D_800A9E88)(NUScTask *);
extern void (*D_800A9E8C)(NUScTask *);
extern u32 D_800C4BD8;
extern u64 D_8009ECB0[];
extern u64 D_8009ED80[];
extern u64 D_800E8710[];
extern u64 D_800C7970[];
extern u64 D_800C6D70[];
extern NUUcode *D_800C4CE0;
extern void *D_800E8210;
extern void **D_800E8290;
extern u32 D_800C4BC4;
extern u32 D_800A9E80;
extern u32 D_800A9E70;
extern u16 D_800A9E90;
extern OSMesgQueue D_800E8B4C;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
void osCreateThread(void *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(void *t);
u32 osSetIntMask(u32 mask);
void osWritebackDCacheAll(void);
void func_80089BC0(void);
void func_80089BE4(u8 active);
void func_80089540(void *arg);
void func_80089660(void);
void func_80089804(void *gfxList_ptr, u32 gfxListSize, u32 ucode, u32 flag) {
    u32 mask;
    D_800B9C80->list.data_ptr = gfxList_ptr;
    D_800B9C80->list.data_size = gfxListSize;
    D_800B9C80->list.flags = flag >> 16;
    D_800B9C80->list.ucode = D_800C4CE0[ucode].ucode;
    D_800B9C80->list.ucode_data = D_800C4CE0[ucode].ucode_data;
    D_800B9C80->flags = flag & 0xFFFF;
    D_800B9C80->framebuffer = D_800E8210;
    D_800B9C80->list.output_buff_size = (u64 *)(D_800A9E70 + (u32)D_800C7970);
    if (D_800A9E90 & 4) {
        D_800A9E90 ^= 4;
        D_800B9C80->list.flags |= 2;
    }
    D_800A9E90 = flag;
    if (flag & 1) {
        D_800B9C80->msg = &D_800B9C86;
        D_800C4BC4 = (D_800C4BC4 + 1) % D_800A9E80;
        D_800E8210 = D_800E8290[D_800C4BC4];
    } else {
        D_800B9C80->msg = &D_800B9C84;
    }
    mask = osSetIntMask(1);
    D_800E79A4++;
    osSetIntMask(mask);
    osWritebackDCacheAll();
    osSendMesg(&D_800E8B4C, D_800B9C80, 1);
    D_800B9C80 = D_800B9C80->next;
}
