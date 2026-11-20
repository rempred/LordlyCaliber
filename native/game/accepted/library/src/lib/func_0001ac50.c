typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef void *OSMesg;
typedef struct {
    struct OSThread_s *mtqueue;
    struct OSThread_s *fullqueue;
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
} OSMesgQueue;
typedef struct {
    u64 at, v0, v1, a0, a1, a2, a3;
    u64 t0, t1, t2, t3, t4, t5, t6, t7;
    u64 s0, s1, s2, s3, s4, s5, s6, s7;
    u64 t8, t9, gp, sp, s8, ra;
    u64 lo, hi;
    u32 sr, pc, cause, badvaddr, rcp;
    u32 fpcsr;
    u64 fp[16];
} __OSThreadContext;
typedef struct OSThread_s {
    struct OSThread_s *next;
    s32 priority;
    struct OSThread_s **queue;
    struct OSThread_s *tlnext;
    u16 state;
    u16 flags;
    s32 id;
    s32 fp;
    __OSThreadContext context;
} OSThread;
typedef struct {
    s32 status;
    OSMesgQueue *queue;
    s32 channel;
    u8 id[32];
    u8 label[32];
    s32 version;
    s32 dir_size;
    s32 inode_table;
    s32 minode_table;
    s32 dir_table;
    s32 inode_start_page;
    u8 banks;
    u8 activebank;
} OSPfs;
typedef s16 NUScMsg;
typedef struct {
    NUScMsg mesg;
    OSMesgQueue *rtnMesgQ;
    s32 error;
    void *dataPtr;
} NUSiCommonMesg;
typedef struct {
    OSPfs *pfs;
    s32 file_no;
    s32 error;
    u32 type;
} NUContPakFile;
typedef struct {
    NUContPakFile *file;
    u8 *noteName;
    u8 *extName;
    s32 size;
    s32 mode;
} NUContPakFileOpenMesg;
extern u16 D_800AA010;
extern u32 D_800AA014;
s32 func_000263c0(OSPfs *pfs, u16 company_code, u32 game_code, u8 *game_name, u8 *ext_name);
s32 func_0001ac50(NUSiCommonMesg *mesg) {
    NUContPakFileOpenMesg *delMesg;
    delMesg = (NUContPakFileOpenMesg *)mesg->dataPtr;
    return func_000263c0(delMesg->file->pfs, D_800AA010, D_800AA014, delMesg->noteName, delMesg->extName);
}
