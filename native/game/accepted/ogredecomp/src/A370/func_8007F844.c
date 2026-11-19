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
               u16 flags;
               u8 flags2;
               u8 flags3;
               u8 unk04[2];
               s16 x;
               s16 y;
               s16 x2;
               s16 y2;
               u16 unk0E;
               void *owner;
               u8 unk14[0x14];
               s16 unk28;
               s16 unk2A;
               u8 unk2C[0xC];
               void *slotsA[10];
               void *slotsB[10];
               u8 unk88;
               u8 unk89;
               u8 unk8A;
               u8 unk8B;
               u8 unk8C;
               u8 unk8D;
               u8 unk8E;
               u8 unk8F;
               u8 unk90[4];
               u8 unk94;
               u8 unk95;
               u8 unk96;
               u8 unk97;
               u8 unk98[0xA];
               s16 parent;
               u8 unkA4[4];
} Scene;
extern Scene D_800E82C8[];
typedef struct {
               u8 *hdr;
               u8 *ptrs[1];
} ArcCtx;
typedef struct {
               u8 unk00[4];
               void *reply;
               u8 type;
               u8 *src;
               u8 *dst;
} ArcMsg;
typedef struct {
               u8 open;
               void *name;
               u8 *buf;
               s32 size;
               s32 pos;
               s32 unk14;
} ArcFile;
typedef struct {
    u16 (*decode_c)(void);
    u16 (*decode_p)(void);
    void (*decode_start)(void);
} DecodeOption;
typedef struct {
               ArcFile *D_800AF36C;
               ArcFile *D_800AF370;
               u32 original;
               s32 packed;
               s32 D_800AF39C;
               s32 method;
} Interface;
extern ArcFile *D_800AF370;
extern u32 D_800AF394;
extern DecodeOption D_800AF3B4;
extern DecodeOption D_800A876C[];
extern u16 D_800AF3C0;
extern s32 D_800AF3AC;
extern u16 D_800AF3B0;
void func_8007C378(u8 *p, u32 n, ArcFile *f);
extern Interface D_800AF378;
void func_8007BF10(Interface *iface);
s32 func_8007C590(ArcFile *f1, ArcFile *f2, s32 size, s32 crc_flg);
extern u16 *D_800AF3FC;
extern u16 *D_800AF400;
void func_8007C74C(s16 D_800AF3E0, u8 nbit, s16 i_special);
void func_8007CAB8(void);
void func_8007E6DC(void);
extern u32 D_800AF3A0;
void func_8007DDF0(s32 p);
extern s32 D_800AF3D8;
extern s16 *D_800AF41C;
extern u8 *D_800AF404;
extern u16 *D_800AF40C;
extern u8 *D_800AF408;
extern u16 *D_800AF414;
extern u16 *D_800AF410;
extern s32 D_800A87CC;
extern s32 D_800A87D0[];
extern s32 D_800A880C;
extern s32 D_800A8810[];
extern u16 *D_800AF430;
extern s16 *D_800AF418;
extern s16 *D_800AF42C;
extern s16 *D_800AF420;
extern s16 *D_800AF428;
extern s16 *D_800AF424;
extern s32 D_800AF3D4;
extern s32 D_800AF3DC;
extern u16 D_800AF3D0;
extern u32 D_800AF3E0;
extern u32 D_800AF3E4;
extern u16 D_800AF39C;
void func_8007BC24(s16 nchar, u8 *bitlen, s16 tablebits, u16 *table);
void func_8007D5B8(void);
void func_8007D8A8(s32 start, s32 end);
s32 func_8007DBF4(s32 p);
extern s32 D_800AF3F0;
extern s32 D_800AF3EC;
extern u8 *D_800AF3A8;
extern ArcFile D_800AF340;
extern ArcFile D_800AF358;
s32 func_8007F570(u8 *dst, s32 size, s32 n, ArcFile *f);
s32 func_8007F5D8(u8 *src, s32 size, s32 n, ArcFile *f);
void *func_8007F4B0(void *d, s32 c, u32 n);
extern u16 D_800AF3C2;
extern u8 D_800AF3C4;
extern u8 D_800AF3C5;
extern s32 D_800AF398;
extern ArcFile *D_800AF36C;
typedef struct {
               void *name;
               u8 unk04[4];
               u8 *buf;
               s32 size;
} ArcReq;
typedef struct {
                u8 header_size;
                signed char method[5];
                u8 unk06[2];
                s32 packed_size;
                s32 original_size;
                u8 unk10[6];
                u8 name[0x100];
                u16 D_800AF3C0;
                s32 has_crc;
                u8 extend_type;
                u8 unk11D[7];
                u16 unix_mode;
                u8 unk126[0xA];
} LzHeader;
typedef struct {
               signed char *name;
               s32 index;
               u8 *buf;
               s32 size;
} ExtractReq;
typedef struct {
    void **ptr;
    s32 size;
} BufDesc;
extern BufDesc D_800A884C[];
extern u8 D_800AF320[];
extern u8 D_800AF300[];
extern u8 D_800AF0D0[0x1B0];
extern u64 D_800AF280[0x80 / 8];
extern u8 D_800AE0A8[];
extern s32 D_800E79A0;
s32 func_80076F5C(s32 a, s32 b, s16 x, s16 y, s16 x2, s16 y2, s32 c);
extern signed char D_800AE0AC[];
extern signed char D_800AE2AC[];
extern signed char D_800AE2D4[];
extern signed char *D_800A8750[];
s32 _bcmp(void *a, void *b, s32 n);
s32 func_8007BA98(ArcFile *infp, ArcFile *outfp, s32 original_size, s32 packed_size, signed char *name, s32 method);
u8 *func_8007F450(u8 *d, u8 *s);
extern signed char D_800AE0C8[];
void extract_one(ArcFile *f, LzHeader *h);
void func_8007BB90(s32 a, s32 b);
void func_8007BBF4(s32 a, s32 b);
s32 func_8007F47C(u8 *a, u8 *b);
void UnpackProc(void *arg);
void osCreateMesgQueue(void *mq, void *msgs, s32 n);
void osCreateThread(void *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);
void osStartThread(void *t);
s32 osRecvMesg(void *mq, void *msg, s32 D_800AF3EC);
u32 DecodeLZ(u8 *dst, u8 *src);
void *HMalloc(s32 size);
void *HFree(void *p);
s32 get_header(ArcFile *f, LzHeader *h);
void func_8007AF3C(s32 alloc);
void func_8007C204(void);
void func_8007BBC0(s32 a);
ArcFile *func_8007F4E4(void *name, u8 *mode, u8 *buf, s32 size);
extern u8 *D_800AF390;
extern u16 *D_800AF3F8;
extern u16 D_800AF3C6;
extern s32 D_800AF3CC;
extern u16 D_800AF3C8;
extern u16 D_800AF39E;
extern u32 D_800AF3E8;
extern u32 D_800AF3F4;
extern u16 D_800AF3A4;
extern signed char D_800AE30C[];
extern signed char D_800AE31C[];
extern signed char D_800AE324[];
extern signed char D_800AE334[];
s32 osSendMesg(void *mq, void *msg, s32 D_800AF3EC);
void osSyncPrintf(const signed char *fmt, ...);
u16 func_8007C25C(u8 n);
u32 ReadU32BE(u8 *p);
void func_8007C438(void);
extern inline void *func_80079F70(void *dst, void *src, u32 n) {
    u8 *d;
    u8 *s;
    u32 i;
    if (n == 0) {
        return dst;
    }
    d = dst;
    s = src;
    if (((u32)s - (u32)d) & 3) {
        while (n-- != 0) {
            *d++ = *s++;
        }
        return dst;
    }
    if ((u32)d & 1) {
        *d++ = *s++;
        n--;
    }
    if (n >= 2 && ((u32)d & 2)) {
        *(u16 *)d = *(u16 *)s;
        s += 2;
        n -= 2;
        d += 2;
    }
    i = n >> 2;
    while (i-- != 0) {
        *(u32 *)d = *(u32 *)s;
        s += 4;
        d += 4;
    }
    if (n & 2) {
        *(u16 *)d = *(u16 *)s;
        s += 2;
        d += 2;
    }
    if (n & 1) {
        *d = *s;
    }
    return dst;
}
extern inline void *func_8007A070(void *dst, s32 c, u32 n) {
    u8 *d;
    u32 i;
    if (n == 0) {
        return dst;
    }
    c &= 0xFF;
    c |= c << 8;
    c |= c << 16;
    d = dst;
    if ((u32)d & 1) {
        *d = c;
        d++;
        n--;
    }
    if (n >= 2 && ((u32)d & 2)) {
        *(u16 *)d = c;
        d += 2;
        n -= 2;
    }
    i = n >> 2;
    while (i-- != 0) {
        *(u32 *)d = c;
        d += 4;
    }
    if (n & 2) {
        *(u16 *)d = c;
        d += 2;
    }
    if (n & 1) {
        *d = c;
    }
    return dst;
}
u32 ReadU32BE(u8 *p);
s32 func_8007AB30(ArcCtx *c, s32 i);
void func_8007AB7C(ArcMsg *m);
void func_8007ABAC(s32 id, s32 pri);
void UnpackProc(void *arg);
extern const signed char D_800AE05C[];
s32 func_8007AE9C(ArcReq *r);
void func_8007AF3C(s32 alloc);
s32 func_8007B710(void);
s32 func_8007B744(void);
void func_8007B7A8(u8 *name, s32 len);
void func_8007B83C(u8 *s, s32 n);
s32 func_8007BA98(ArcFile *infp, ArcFile *outfp, s32 original_size, s32 packed_size, signed char *name, s32 method);
void func_8007BB48(u8 *s, s32 n);
void func_8007BB90(s32 a, s32 b);
void func_8007BBC0(s32 a);
void func_8007BBF4(s32 a, s32 b);
void func_8007BC24(s16 nchar, u8 *bitlen, s16 tablebits, u16 *table);
void func_8007BF10(Interface *iface);
void func_8007C204(void);
extern inline void fillbuf(u8 n) {
    while (n > D_800AF3C5) {
        n -= D_800AF3C5;
        D_800AF3C2 = (D_800AF3C2 << D_800AF3C5) + (D_800AF3C4 >> (8 - D_800AF3C5));
        if (D_800AF398 != 0) {
            D_800AF398--;
            D_800AF3C4 = D_800AF36C->buf[D_800AF36C->pos++];
        } else {
            D_800AF3C4 = 0;
        }
        D_800AF3C5 = 8;
    }
    D_800AF3C5 -= n;
    D_800AF3C2 = (D_800AF3C2 << n) + (D_800AF3C4 >> (8 - n));
    D_800AF3C4 <<= n;
}
u16 func_8007C25C(u8 n);
extern inline void calccrc(u8 *p, s32 n) {
    while (n-- != 0) {
        D_800AF3C0 = D_800AF3F8[(D_800AF3C0 ^ *p++) & 0xFF] ^ (D_800AF3C0 >> 8);
    }
}
void func_8007C378(u8 *p, u32 n, ArcFile *fp);
void func_8007C438(void);
u8 *func_8007C538(u8 *s, u8 sep);
s32 func_8007C590(ArcFile *f1, ArcFile *f2, s32 size, s32 crc_flg);
void func_8007C74C(s16 D_800AF3E0, u8 nbit, s16 i_special);
void func_8007CAB8(void);
u16 func_8007CE48(void);
u16 func_8007D200(void);
void func_8007D594(void);
void func_8007D5B8(void);
void func_8007D7BC(void);
void func_8007D8A8(s32 start, s32 end);
s32 func_8007DBF4(s32 p);
extern inline void update_p_inline(s32 p) {
    s32 q;
    if (D_800AF3D0 == 0x8000) {
        func_8007D8A8(628, D_800AF3DC + 1);
        D_800AF3D0 = D_800AF430[628];
        D_800AF430[628] = 0xFFFF;
    }
    q = D_800AF42C[p + 314];
    while (q != 628) {
        q = func_8007DBF4(q);
    }
    D_800AF3D0++;
}
void func_8007DDF0(s32 p);
extern inline void update_c_inline(s32 p) {
    s32 q;
    if (D_800AF430[0] == 0x8000) {
        func_8007D8A8(0, D_800AF3CC * 2 - 1);
    }
    D_800AF430[0]++;
    q = D_800AF42C[p];
    do {
        q = func_8007DBF4(q);
    } while (q != 0);
}
u16 func_8007DFF0(void);
u16 func_8007E2F8(void);
void func_8007E698(void);
void func_8007E6DC(void);
void func_8007E800(void);
u16 func_8007E8F0(void);
u16 func_8007EE2C(void);
u16 func_8007F1A0(void);
u16 func_8007F1F8(void);
void func_8007F218(void);
u16 func_8007F234(void);
u16 func_8007F314(void);
void func_8007F334(void);
u8 *func_8007F408(u8 *d, u8 *s);
u8 *func_8007F450(u8 *d, u8 *s);
s32 func_8007F47C(u8 *a, u8 *b);
void *func_8007F4B0(void *d, s32 c, u32 n);
ArcFile *func_8007F4E4(void *name, u8 *mode, u8 *buf, s32 size);
s32 func_8007F570(u8 *dst, s32 size, s32 n, ArcFile *f);
s32 func_8007F5D8(u8 *src, s32 size, s32 n, ArcFile *f);
void func_8007F640(void);
s32 func_8007F648(s32 a, s16 x, s16 y, s16 b, u16 c);
s32 func_8007F844(u8 *s) {
    s32 n = 0;
    u8 c;
    while ((c = *s++) != 0) {
        s += c >> 7;
        n++;
    }
    return n;
}
