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
typedef struct HeapNode {
               struct HeapNode *prev;
               struct HeapNode *next;
               struct HeapNode **parent;
               struct HeapNode *left;
               struct HeapNode *right;
               u32 used;
               u32 free;
               u8 unk1C[4];
} HeapNode;
typedef struct {
               HeapNode *start;
               HeapNode *end;
               HeapNode *root;
} Heap;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Heap gHeaps[];
typedef struct {
               u8 unk00[0xC];
               void *fb;
} FrameMsg;
typedef struct {
               void *p[5];
               Gfx *gfx;
} FbSet;
extern void *D_800C4BB8;
typedef struct {
               u8 *base;
               s32 stride;
               u8 *fields;
               u32 count;
} SaveDesc;
typedef struct {
               void *unk0;
               void (*update)(void);
               void (*draw)(FbSet *);
} SceneFuncs;
extern FbSet D_800A81C0[2];
extern SceneFuncs *D_800E8294;
extern u8 nuScPreNMIFlag;
extern u8 D_800AEF98;
extern volatile u16 D_800C4C26;
extern u8 D_800E810C;
extern u8 D_800E7A0C;
extern u8 D_800C4CE4;
extern u8 D_800A81FE;
extern u8 D_800A81FF;
extern u8 D_800A8200;
extern u8 D_800A8214;
extern u8 D_800A81FC;
extern u8 D_800AEFA0;
extern u32 D_800AEFA4;
extern u8 gPlayTime[];
u8 func_80072944(void);
void func_80089BC0(void);
void func_80073ED8(void);
void func_800EAC24(void);
void func_800E9E34(void);
void func_800EAF1C(void);
void func_800733F8(void);
void func_80073BD0(void);
void func_80073AE4(void);
void func_80076AE8(void);
void func_80180BDC(void);
void func_8007297C(void);
void func_8016CB44(void *t);
void func_8007FD10(void);
void func_80076DC8(void);
void func_800765D8(void);
void func_8017F490(void);
void func_801841F4(void);
extern u16 D_800C4C08;
extern u16 D_800E7D68;
extern u16 D_800C4A18;
extern u16 D_800E7A1C;
extern u16 D_800C4BCA;
extern u16 D_800C4AD8;
extern u8 D_800AEE72;
extern u8 D_800A81F0;
extern FbSet *D_800E9BE0;
extern s16 D_800E79E0;
extern s16 D_800E9BC4;
extern s16 D_800C4BE4;
extern s16 D_800C4BC8;
extern s16 D_800C4C48;
extern u32 D_800C4BC4;
extern u32 D_800A9E80;
extern u8 D_800A8215;
extern u8 D_800AEDF0[];
extern u8 D_800AEE30[];
extern u16 D_800AEE70;
void *HMallocBack(s32 size);
void func_800737A0(void);
void func_8007382C(void);
void guPerspective(void *m, u16 *perspNorm, f32 fovy, f32 aspect, f32 near, f32 far, f32 scale);
void guLookAt(void *m, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp);
extern u8 D_800C4808;
u8 func_8016CD3C(void);
void osViSwapBuffer(void *fb);
void func_80089804(Gfx *start, s32 size, u8 a, s32 b);
void func_80073C48(void);
extern u32 D_800C4818;
extern u8 osViModeNtscLan1[];
extern u8 osViModeMpalLan1[];
extern u8 D_800AB9B0[];
extern u8 D_800ABA50[];
extern u16 D_800AEF10[4][16];
extern u16 D_800AEE90[4][16];
typedef struct {
    u8 b[6];
} Unk6;
typedef struct {
    s16 v[9];
} Unk12;
extern Unk6 D_800AEE78[4];
extern Unk6 *D_800C47F0[4];
extern Unk12 D_800E9C08;
extern Unk12 D_800ABD80;
extern u8 D_800E82C4;
extern u8 D_80243DB0[];
extern u8 ovl_03F1B0_VRAM[];
extern u8 ovl_03F1B0_TEXT_START[];
extern u8 ovl_03F1B0_TEXT_END[];
extern u8 ovl_03F1B0_DATA_START[];
extern u8 ovl_03F1B0_RODATA_END[];
extern u8 ovl_03F1B0_BSS_START[];
extern u8 ovl_03F1B0_BSS_END[];
extern u8 ovl_03F1B0_ROM_START[];
extern u8 ovl_03F1B0_ROM_END[];
extern u8 ovl_040E80_VRAM[];
extern u8 ovl_040E80_TEXT_START[];
extern u8 ovl_040E80_TEXT_END[];
extern u8 ovl_040E80_DATA_START[];
extern u8 ovl_040E80_RODATA_END[];
extern u8 ovl_040E80_BSS_START[];
extern u8 ovl_040E80_BSS_END[];
extern u8 ovl_040E80_ROM_START[];
extern u8 ovl_040E80_ROM_END[];
extern u8 D_800A8204[];
extern u16 D_800E8214;
extern u16 D_800E79B0;
extern s32 D_8018FDC0;
extern u8 gFlags[];
void func_800E9C20(void);
void osAiSetFrequency(u32 f);
void osInvalICache(void *p, s32 n);
void osInvalDCache(void *p, s32 n);
void RomRead(void *rom, void *dst, s32 size);
void MusSetMasterVolume(s32 flags, s32 volume);
void func_8008A1B0(void);
void func_80089C60(void);
u8 func_8008A260(void);
void func_8008A660(void (*f)(void));
void func_8008A040(void);
void func_800E9CEC(void);
void func_800EA714(void);
void func_80089AB0(void *a, s32 b);
void func_800899D0(void (*f)(FrameMsg *));
void func_8009DAEC(void);
void func_80079380(void);
void func_80179060(void);
void func_800EA8E0(void);
void func_80173610(void);
void func_80080280(Gfx **head);
void func_8017BDC0(void);
void func_80184D70(void);
void func_8016C900(void);
void func_800746C8(void);
void func_800749C0(s32 a);
void func_800764E0(void);
void func_8016CD50(s32 a);
void func_80075BC0(void);
void func_80089A30(s32 n);
s32 func_80073F58(void);
void func_80073FD4(void);
s32 func_800819D4(s32 a);
void func_80072710(void);
void func_80072738(s32 mode);
u8 func_80072900(void);
u8 func_800728BC(void);
void func_800727D8(void);
void func_8007284C(void);
void func_8007307C(FrameMsg *msg);
void func_80073164(u8 relative, s16 a, s16 b, s16 c, s16 d, s16 e, s16 f);
void func_80070C60(u32 start, s32 size);
extern u16 D_800C4BF0;
extern Gfx *D_800E9BA0;
extern u8 D_80186E70[];
extern u8 D_801869C8[];
void bzero(void *p, s32 n);
void osSyncPrintf(const signed char *fmt, ...);
void _bcopy(void *src, void *dst, s32 n);
void osViSetMode(void *m);
extern u16 gHeapCount;
extern u16 gHeapAllocFlags;
extern s32 osTvType;
extern u8 D_800AEF9A;
extern u8 D_800A8210[];
extern u8 D_800A8211;
extern u8 D_800A8212;
extern u8 D_800A8213;
extern void *D_800AEF9C;
extern u32 D_800C4B20;
extern void *D_800E8210;
u32 osVirtualToPhysical(void *p);
extern u8 *D_800AEFB0;
extern s32 D_800AEFB4;
extern s32 D_800AEFB8;
extern s32 D_800AEFBC;
extern u32 D_800AEFC0;
extern u8 D_800AEF99;
extern s32 D_800A81F4;
extern u32 D_800A81F8;
extern u32 nuScRetraceCounter;
extern s32 D_800EB0DC;
extern s32 D_800EB12C;
extern s32 D_800EB17C;
extern s32 D_800EB1CC;
extern s32 D_800EB21C;
extern s32 D_800EB26C;
extern s32 D_800EB2BC;
extern s32 *D_800A8218[];
void osViSetSpecialFeatures(u32 f);
void func_80073CB0(u8 alpha);
u8 func_8016CD30(void);
extern u8 D_80186358[];
extern u8 D_80186610[];
void func_80081908(s32 a, s32 b);
void *HMalloc(s32 size);
void *HFree(void *p);
s32 HFindHeap(u32 p);
void func_80071950(HeapNode *n);
void func_800719E8(HeapNode **slot, HeapNode *n);
HeapNode *func_80071A3C(HeapNode *n, u32 size);
void func_80071A74(void);
HeapNode *func_80071E74(HeapNode *n, u32 size);
u32 osSetIntMask(u32 mask);
void nuGfxTaskAllEndWait(void);
void func_8007328C(void);
void func_80070C60(u32 start, s32 size);
void func_80070F14(void);
void *HMalloc(s32 size);
void *func_800710DC(s32 size);
void *HMallocBack(s32 size);
void *HFree(void *ptr);
u32 func_800713EC(void);
s32 HIsAllocatedPointer(void *ptr);
extern const signed char D_800ADE6C[];
s32 func_80071634(s32 *p);
s32 func_8007163C(void);
extern inline void *HFreeInline(void *ptr) {
    s32 h = HFindHeap((u32)ptr);
    HeapNode *b;
    if (ptr == ((void *)0)) {
        return ((void *)0);
    }
    b = (HeapNode *)ptr - 1;
    if (b->free > 0x20) {
        func_80071950(b);
    }
    ((HeapNode *)ptr - 1)->prev->next = ((HeapNode *)ptr - 1)->next;
    if (((HeapNode *)ptr - 1)->next < gHeaps[h].end) {
        ((HeapNode *)ptr - 1)->next->prev = ((HeapNode *)ptr - 1)->prev;
    }
    b = ((HeapNode *)ptr - 1)->prev;
    if (b->free > 0x20) {
        func_80071950(b);
    }
    b->free = (u8 *)b->next - ((u8 *)b + (b->used + sizeof(HeapNode)));
    if (b->free > 0x20) {
        func_800719E8(&gHeaps[h].root, b);
    }
    ((HeapNode *)ptr - 1)->next = ((void *)0);
    ((HeapNode *)ptr - 1)->prev = ((void *)0);
    if (++D_800C4818 > 0x3FFFF) {
        func_80071A74();
        D_800C4818 = 0;
    }
    return ((void *)0);
}
void *func_80071644(void *ptr, s32 size);
void func_80071950(HeapNode *n);
void func_800719E8(HeapNode **slot, HeapNode *n);
HeapNode *func_80071A3C(HeapNode *n, u32 size);
void func_80071A74(void);
s32 HFindHeap(u32 p);
extern inline void TreeRemoveInline(HeapNode *n) {
    HeapNode *p;
    if (n->left == ((void *)0)) {
        if (n->right != ((void *)0)) {
            *n->parent = n->right;
            n->right->parent = n->parent;
        } else {
            *n->parent = ((void *)0);
        }
    } else {
        *n->parent = n->left;
        n->left->parent = n->parent;
        if (n->right != ((void *)0)) {
            p = n->left;
            while (p->right != ((void *)0)) {
                p = p->right;
            }
            p->right = n->right;
            n->right->parent = &p->right;
        }
    }
}
void *func_80071C04(s32 size);
HeapNode *func_80071E74(HeapNode *n, u32 size);
void func_80072398(s32 retrace);
void func_80072710(void);
void func_80072738(s32 mode);
void func_800727D8(void);
void func_8007284C(void);
u8 func_800728BC(void);
u8 func_80072900(void);
u8 func_80072944(void);
void func_8007307C(FrameMsg *msg);
void func_80073164(u8 relative, s16 a, s16 b, s16 c, s16 d, s16 e, s16 f);
void func_8007328C(void);
void func_8007337C(void);
u8 func_8007338C(void);
void func_80073398(void);
void func_800737A0(void);
void func_8007382C(void) {
    if (func_8016CD30()) {
        return;
    }
    if (D_800A8213) {
        Gfx *g = D_800E9BA0;
        D_800E9BA0 = g + 1;
        g[0].w0 = 0xFE000000;
        g[0].w1 = D_800C4B20 - 0x80000000;
        D_800E9BA0 = g + 2;
        g[1].w0 = 0xE7000000;
        g[1].w1 = 0;
        D_800E9BA0 = g + 3;
        g[2].w0 = 0xE7000000;
        g[2].w1 = 0;
        D_800E9BA0 = g + 4;
        g[3].w0 = 0xE3000A01;
        g[3].w1 = 0x300000;
        D_800E9BA0 = g + 5;
        g[4].w0 = 0xE7000000;
        g[4].w1 = 0;
        D_800E9BA0 = g + 6;
        g[5].w0 = 0xE200001C;
        g[5].w1 = 0;
        D_800E9BA0 = g + 7;
        g[6].w0 = 0xFF10013F;
        g[6].w1 = D_800C4B20 - 0x80000000;
        D_800E9BA0 = g + 8;
        g[7].w0 = 0xF7000000;
        g[7].w1 = 0xFFFCFFFC;
        D_800E9BA0 = g + 9;
        g[8].w0 = 0xE7000000;
        g[8].w1 = 0;
        D_800E9BA0 = g + 10;
        g[9].w0 = 0xF64FC3BC;
        g[9].w1 = 0;
        D_800E9BA0 = g + 11;
        g[10].w0 = 0xE7000000;
        g[10].w1 = 0;
        D_800E9BA0 = g + 12;
        g[11].w0 = 0xE7000000;
        g[11].w1 = 0;
    }
    {
        Gfx *g = D_800E9BA0;
        D_800E9BA0 = g + 1;
        g[0].w0 = 0xE7000000;
        g[0].w1 = 0;
        D_800E9BA0 = g + 2;
        g[1].w0 = 0xE7000000;
        g[1].w1 = 0;
        D_800E9BA0 = g + 3;
        g[2].w0 = 0xE3000A01;
        g[2].w1 = 0x300000;
        D_800E9BA0 = g + 4;
        g[3].w0 = 0xE7000000;
        g[3].w1 = 0;
        D_800E9BA0 = g + 5;
        g[4].w0 = 0xE200001C;
        g[4].w1 = 0;
        D_800E9BA0 = g + 6;
        g[5].w0 = 0xFF10013F;
        g[5].w1 = osVirtualToPhysical(D_800E8210);
    }
    {
        Gfx *g = D_800E9BA0;
        D_800E9BA0 = g + 1;
        g[0].w0 = 0xF7000000;
        g[0].w1 = 0x10001;
        D_800E9BA0 = g + 2;
        g[1].w0 = 0xE7000000;
        g[1].w1 = 0;
        D_800E9BA0 = g + 3;
        g[2].w0 = 0xF64FC3BC;
        g[2].w1 = 0;
        D_800E9BA0 = g + 4;
        g[3].w0 = 0xE7000000;
        g[3].w1 = 0;
        D_800E9BA0 = g + 5;
        g[4].w0 = 0xDE000000;
        g[4].w1 = (u32)D_801869C8;
    }
}
void func_80073AE4(void);
void func_80073BD0(void);
void func_80073C48(void);
void func_80073CB0(u8 alpha);
void func_80073ED8(void);
s32 func_80073F58(void);
void func_80073FD4(void);
void func_80074050(u8 *a);
u32 func_80074080(void);
u32 func_800740F0(s32 n);
void func_800741A8(s32 bit);
void func_8007422C(s32 n, u32 val);
extern inline void BitInitInline(u8 *a) {
    D_800AEFB0 = a;
    D_800AEFB4 = 0;
    D_800AEFB8 = 0;
    D_800AEFBC = 8;
    D_800AEFC0 = 0;
}
extern inline u32 BitReadInline(s32 n) {
    u32 r = 0;
    while (D_800AEFB8 < n) {
        n -= D_800AEFB8;
        r |= (D_800AEFC0 & ((1 << D_800AEFB8) - 1)) << n;
        D_800AEFC0 = D_800AEFB0[D_800AEFB4++];
        D_800AEFB8 = 8;
    }
    D_800AEFB8 -= n;
    return r | ((D_800AEFC0 >> D_800AEFB8) & ((1 << n) - 1));
}
extern inline void BitWriteInline(s32 n, u32 val) {
    while (n >= D_800AEFBC) {
        n -= D_800AEFBC;
        D_800AEFC0 |= (val >> n) & ((1 << D_800AEFBC) - 1);
        D_800AEFB0[D_800AEFB4++] = D_800AEFC0;
        D_800AEFC0 = 0;
        D_800AEFBC = 8;
    }
    D_800AEFBC -= n;
    D_800AEFC0 |= (val & ((1 << n) - 1)) << D_800AEFBC;
}
void func_800742F4(u8 *buf, SaveDesc *d);
void func_80074494(u8 *buf, SaveDesc *d);
