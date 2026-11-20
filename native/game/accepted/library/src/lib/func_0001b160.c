typedef signed char s8;
typedef unsigned char u8;
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
typedef struct {
    OSPfs *pfs;
    s32 file_no;
    s32 error;
    u32 type;
} NUContPakFile;
void bzero(void *p, s32 n);
void encode_ascii_to_glyph(u8 *jis, u8 *n64, u32 len);
void func_0001b0a0(NUContPakFile *file, u8 *noteName, u8 *extName, u32 mode, s32 size);
void func_0001b160(NUContPakFile *file, u8 *noteName, u8 *extName, u32 mode, s32 size) {
    u8 noteNameN64[32];
    u8 extNameN64[4];
    u32 len;
    if (file->type == 0) {
        file->error = 1;
        return;
    }
    bzero(noteNameN64, 32);
    bzero(extNameN64, 4);
    for (len = 0; noteName[len] != 0; len++) {
    }
    encode_ascii_to_glyph(noteName, noteNameN64, len);
    encode_ascii_to_glyph(extName, extNameN64, 1);
    func_0001b0a0(file, noteNameN64, extNameN64, mode, size);
}
