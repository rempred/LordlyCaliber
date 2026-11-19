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
typedef struct { u8 b[0x76]; } ScriptObj;
typedef struct { s32 used; u8 b[0x58]; } ScriptRes;
typedef struct { u8 b; u8 pad[31]; } Rec32;
extern ScriptObj D_8022A2E8_ovl_1BA050[];
extern ScriptRes D_8022A4E0_ovl_1BA050[];
extern Rec32 D_8022A6A1_ovl_1BA050[];
extern s16 D_8022A718_ovl_1BA050;
extern s16 D_8022A71A_ovl_1BA050;
extern s16 D_8022A71C_ovl_1BA050;
extern s16 D_8022A71E_ovl_1BA050;
void bzero(void *p, s32 size);
void *func_8017F334(u16 id);
void func_8017C2FC(void *p);
void func_80225058_ovl_1BA050(s32 a, s32 b);
void func_802261D0_ovl_1BA050(s32 a, s32 b);
ScriptObj *func_80222EA0_ovl_1BA050(s32 i);
ScriptObj *func_80222EEC_ovl_1BA050(s32 i);
void func_80224AAC_ovl_1BA050();
void func_80180B1C(u8 *n, s16 x0, s16 y0, s16 x1, s16 y1);
s32 func_801794C8(void *text);
void func_802284B4_ovl_1BA050();
void func_80228BF0_ovl_1BA050(s32 id);
void func_8017C4B4(s16 id);
void func_80224E74_ovl_1BA050(s16 x0, s16 y0, s16 x1, s16 y1);
void func_80179034(void *s, s32 x, s32 y);
extern u8 D_8022A1D8_ovl_1BA050[];
s32 func_8017B550(u8 c);
extern u8 D_8022A0A8_ovl_1BA050;
extern u8 D_8022A0A4_ovl_1BA050;
extern u8 D_8022A09D_ovl_1BA050;
extern u8 D_8022A09C_ovl_1BA050;
extern u8 D_8022A099_ovl_1BA050;
extern u8 D_8022A098_ovl_1BA050;
typedef struct {
               s32 used;
               void *text;
               u8 flags;
               u8 state;
               s16 hA;
               s16 hC;
               u16 hE;
               u16 h10;
               u16 h12;
               u16 h14;
               s16 h16;
               s16 h18;
               s16 h1A;
               s16 h1C;
               u8 unk1E[2];
} Rec698;
extern Rec698 D_8022A698_ovl_1BA050[];
extern u16 D_8022A4D8_ovl_1BA050;
extern u16 D_8022A4DA_ovl_1BA050;
extern u16 D_8022A4DC_ovl_1BA050;
extern u16 D_8022A4DE_ovl_1BA050;
extern u16 D_800E79B0;
extern u16 D_800E9BE4;
extern void *D_8022A4C0_ovl_1BA050[];
extern u8 *D_8022A650_ovl_1BA050;
extern s32 D_8022A838_ovl_1BA050;
extern u8 D_8022A820_ovl_1BA050;
extern u8 D_80229F9C_ovl_1BA050[];
extern u8 D_8022A834_ovl_1BA050;
extern s16 D_8022A832_ovl_1BA050;
void HFree(void *p);
void func_80220F64_ovl_1BA050(u16 a, u8 b);
u8 *func_8017C074(u16 a, u8 b, void *fn, s16 c, s16 d, s16 e, s16 f);
void func_80229BD4_ovl_1BA050(void);
s32 func_8016FBE0(u8 a, u8 b, void *out);
typedef void (*ScriptObjFn)(ScriptObj *o);
extern ScriptObjFn D_8022A020_ovl_1BA050[];
extern void *D_8022A260_ovl_1BA050[];
extern void *D_8022A654_ovl_1BA050;
extern u8 D_8022A658_ovl_1BA050[];
void *HMalloc(s32 size);
void *LoadChunk(s32 id);
void _bcopy(void *src, void *dst, s32 n);
typedef struct {
    u32 w0;
    u32 w1;
} GfxBB;
extern GfxBB *D_800E9BA0;
extern u8 D_80229FB8_ovl_1BA050[];
extern ScriptObjFn D_80229FA4_ovl_1BA050[];
extern s32 D_8022A720_ovl_1BA050;
extern s16 D_8022A724_ovl_1BA050;
extern s16 D_8022A726_ovl_1BA050;
extern s16 D_8022A728_ovl_1BA050;
extern s16 D_8022A72A_ovl_1BA050;
void func_80227234_ovl_1BA050(u8 *p);
void func_80226E8C_ovl_1BA050(ScriptRes *r);
void func_802292DC_ovl_1BA050(s16 *p);
s32 ChunkSize(s32 id);
void *LoadChunkRaw(void *dst, s32 id);
void *func_8017BFB0(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h, s16 x2, s16 y2, s16 w2, s16 h2);
void func_80229B98_ovl_1BA050();
void func_802261D0_ovl_1BA050();
extern u8 D_8022A821_ovl_1BA050;
extern u8 D_8022A824_ovl_1BA050;
extern s16 D_8022A826_ovl_1BA050;
extern u8 D_8022A833_ovl_1BA050;
extern u8 D_8022A0D0_ovl_1BA050[];
void func_802296B0_ovl_1BA050();
void func_80224268_ovl_1BA050(u8 *p, u16 mask, u8 v);
void func_80224320_ovl_1BA050(u8 *p, u16 mask);
void func_80224A88_ovl_1BA050(void);
void func_80224AAC_ovl_1BA050(void);
void func_80224B5C_ovl_1BA050(void);
void func_80224BE8_ovl_1BA050(void);
void func_80224C60_ovl_1BA050(void);
void func_80224D60_ovl_1BA050(void);
void func_80224E1C_ovl_1BA050(void);
ScriptRes *func_80226164_ovl_1BA050(u32 i);
s32 func_80226198_ovl_1BA050(void);
void func_802263CC_ovl_1BA050(void);
s32 func_80226420_ovl_1BA050(u8 *p);
void func_80226430_ovl_1BA050(u8 *p, s32 a);
s32 func_8022648C_ovl_1BA050(u8 *c);
void func_80227E00_ovl_1BA050(s32 i);
void func_80227EB8_ovl_1BA050(u8 *p, s32 a, s32 b, s32 c);
void func_80227F14_ovl_1BA050(u32 i, s32 v);
extern inline void ScriptResFree(s32 i) {
    ScriptRes *e;
    if (i >= 4) {
        for (;;);
    }
    e = &D_8022A4E0_ovl_1BA050[i];
    if ((*(u8 * *)((u8 *)(e) + (4))) != ((void *)0)) {
        (*(u8 * *)((u8 *)((*(u8 * *)((u8 *)(e) + (4)))) + (8))) = (*(u8 * *)((u8 *)(e) + (8)));
    } else {
        D_8022A650_ovl_1BA050 = (*(u8 * *)((u8 *)(e) + (8)));
    }
    if ((*(u8 * *)((u8 *)(e) + (8))) != ((void *)0)) {
        (*(u8 * *)((u8 *)((*(u8 * *)((u8 *)(e) + (8)))) + (4))) = (*(u8 * *)((u8 *)(e) + (4)));
    }
    if ((*(void * *)((u8 *)(e) + (0x34))) != ((void *)0)) {
        HFree((*(void * *)((u8 *)(e) + (0x34))));
        (*(void * *)((u8 *)(e) + (0x34))) = ((void *)0);
    }
    if ((*(void * *)((u8 *)(e) + (0x4C))) != ((void *)0)) {
        HFree((*(void * *)((u8 *)(e) + (0x4C))));
        (*(void * *)((u8 *)(e) + (0x4C))) = ((void *)0);
    }
    e->used = 0;
}
s32 func_80227F68_ovl_1BA050(s32 i);
void func_802282D0_ovl_1BA050(void);
void func_80228378_ovl_1BA050(void);
void func_8022845C_ovl_1BA050(void);
extern inline Rec698 *Rec698Find(s32 id) {
    Rec698 *r = ((void *)0);
    s32 i;
    for (i = 0; i < 4; i++) {
        if (D_8022A698_ovl_1BA050[i].used != 0 && D_8022A698_ovl_1BA050[i].used == id) {
            r = &D_8022A698_ovl_1BA050[i];
            break;
        }
    }
    return r;
}
void func_80228BF0_ovl_1BA050(s32 id);
void func_80228CE4_ovl_1BA050(void);
void func_80228E88_ovl_1BA050(s32 i);
void func_80228EE4_ovl_1BA050(void);
void func_80228F54_ovl_1BA050(void);
void func_80228FE8_ovl_1BA050(s32 i);
s32 func_80229040_ovl_1BA050(s32 i);
extern inline void func_802291BC_ovl_1BA050(s16 a, s16 b, s16 c, s16 d) {
    D_8022A718_ovl_1BA050 = a;
    D_8022A71A_ovl_1BA050 = b;
    D_8022A71C_ovl_1BA050 = c;
    D_8022A71E_ovl_1BA050 = d;
}
typedef struct {
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
} Rect1BB;
s32 func_802291E0_ovl_1BA050(Rect1BB *a, Rect1BB *b, Rect1BB *out);
extern inline void func_80229604_ovl_1BA050(s16 *list) {
    if (list == ((void *)0)) {
        s32 *p = &D_8022A720_ovl_1BA050;
        bzero(p, 0xF4);
        *p = 1;
        D_8022A72A_ovl_1BA050 = 0xF0;
        D_8022A726_ovl_1BA050 = 0;
        D_8022A724_ovl_1BA050 = 0;
        D_8022A728_ovl_1BA050 = 0x140;
    } else {
        s16 *q = list;
        while (q[1] != 0 || q[3] != 0 || q[0] != 0 || q[2] != 0) {
            func_802292DC_ovl_1BA050(q);
            q += 4;
        }
    }
}
void func_80229828_ovl_1BA050(void);
void func_80229930_ovl_1BA050(void);
extern inline void func_8022995C_reset(void) {
    bzero(&D_8022A720_ovl_1BA050, 0xF4);
    D_8022A720_ovl_1BA050 = 1;
    D_8022A72A_ovl_1BA050 = 0xF0;
    D_8022A726_ovl_1BA050 = 0;
    D_8022A724_ovl_1BA050 = 0;
    D_8022A728_ovl_1BA050 = 0x140;
}
void func_80229A20_ovl_1BA050(void);
void func_80229B18_ovl_1BA050(void);
void func_80229B44_ovl_1BA050(void) {
    D_8022A820_ovl_1BA050 = 0;
    D_8022A834_ovl_1BA050 = 8;
    D_8022A832_ovl_1BA050 = 0;
    func_8017C074(0x5501, 0x13, func_80229BD4_ovl_1BA050, 0, 0, 0, 0);
}
void func_80229B98_ovl_1BA050(s32 a, s32 b);
void func_80229BD4_ovl_1BA050(void);
