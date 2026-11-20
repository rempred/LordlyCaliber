typedef unsigned char u8;
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
typedef struct NUScTask_s {
    struct NUScTask_s *next;
    u32 state;
    u32 flags;
    void *framebuffer;
    OSTask list;
    OSMesgQueue *msgQ;
    OSMesg msg;
} NUScTask;
extern volatile u32 D_800E79A4;
extern OSMesg D_800BBE40[];
extern OSMesgQueue D_800E9BA8;
extern void (*D_800A9E84)(NUScTask *);
extern void (*D_800A9E88)(NUScTask *);
extern void (*D_800A9E8C)(NUScTask *);
extern u32 D_800C4BD8;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 func_00023AE0(OSMesgQueue *mq, OSMesg *msg, s32 flag);
u32 osSetIntMask(u32 mask);
void func_00019fe4(u8 active);
void func_00019940(void *arg) {
    NUScTask *gfxTask;
    s16 *msgtype;
    u32 mask;
    osCreateMesgQueue(&D_800E9BA8, D_800BBE40, 8);
loop:
    {
        func_00023AE0(&D_800E9BA8, (OSMesg *)&gfxTask, 1);
        msgtype = (s16 *)gfxTask->msg;
        switch (*msgtype) {
            case 4:
                if (D_800A9E84 != ((void *)0)) {
                    D_800A9E84(gfxTask);
                }
                if (D_800C4BD8 & 0x80) {
                    func_00019fe4(0);
                    D_800C4BD8 = 1;
                }
                break;
            case 8:
                if (D_800A9E88 != ((void *)0)) {
                    D_800A9E88(gfxTask);
                }
                break;
            case 0x10:
                if (D_800A9E8C != ((void *)0)) {
                    D_800A9E8C(gfxTask);
                }
                break;
            default:
                goto loop;
        }
        mask = osSetIntMask(1);
        D_800E79A4--;
        osSetIntMask(mask);
    }
    goto loop;
}
