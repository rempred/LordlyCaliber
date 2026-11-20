typedef unsigned char u8;
typedef signed int s32;
typedef struct MusLink {
    struct MusLink *next;
    struct MusLink *prev;
} MusLink;
typedef struct MusParam {
    struct MusParam *next;
    u8 pad04[0x18];
} MusParam;
typedef struct {
    void *head;
    MusLink pFreeList;
    MusLink pAllocList;
    MusLink pLameList;
    s32 paramSamples;
    s32 curSamples;
    void *dma;
    void *heap;
    MusParam *paramList;
    struct {
        u8 pad00[4];
        void *handler;
    } *mainBus;
    struct {
        u8 pad00[0x14];
        s32 sourceCount;
        s32 maxSources;
        void **sources;
        void *unk20;
        s32 arr[1];
    } *unk34;
    s32 numPVoices;
    u8 pad3C[4];
    s32 outputRate;
    s32 maxOutSamples;
    s32 sv_dramout;
    s32 sv_first;
} AlGlobalsLike;
extern AlGlobalsLike *D_800E8298;
s32 func_00015560(s32 fx);
extern s32 D_800A9B60;
void func_000151e0(s32 *param);
void func_0001f2a4(void *fx, s32 id, void *param);
void func_000151e0(s32 *param);
extern s32 *D_800A9B64[];
s32 func_00015560(s32 fx) {
    s32 r;
    if (fx < D_800A9B60) {
        func_000151e0(D_800A9B64[fx]);
        func_0001f2a4(D_800E8298->unk34->unk20, 1, D_800E8298->unk34);
        r = 0;
    } else {
        r = 1;
    }
    return r;
}
