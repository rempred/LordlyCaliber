typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
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
    u16 type;
    u8 status;
    u8 errno;
} OSContStatus;
extern OSMesgQueue D_800E9B88;
extern OSMesg D_800BBE60[];
extern OSThread D_800BBE80;
extern u64 D_800BE030[];
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
void osSetEventMesg(s32 event, OSMesgQueue *mq, OSMesg msg);
s32 func_00020870(OSMesgQueue *mq, u8 *bitpattern, OSContStatus *status);
void osCreateThread(OSThread *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(OSThread *t);
void func_0001a19c(void *arg);
u8 func_0001a060(void) {
    u8 pattern;
    OSContStatus status[4];
    osCreateMesgQueue(&D_800E9B88, D_800BBE60, 8);
    osSetEventMesg(5, &D_800E9B88, 0);
    func_00020870(&D_800E9B88, &pattern, status);
    osCreateThread(&D_800BBE80, 5, func_0001a19c, ((void *)0), D_800BE030, 0x73);
    osStartThread(&D_800BBE80);
    return pattern;
}
void func_0001a19c(void *arg);
