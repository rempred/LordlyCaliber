typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    s32 maxVVoices;
    s32 maxPVoices;
    s32 maxUpdates;
    s32 maxFXbusses;
    void *dmaproc;
    void *heap;
    s32 outputRate;
    u8 fxType;
    s32 *params;
} MusSynConfig;
void func_000154c0(MusSynConfig *p);
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
extern AlGlobalsLike *D_800A9E54;
extern AlGlobalsLike *D_800A9E50;
extern AlGlobalsLike *D_800E8298;
void func_00014dd8(MusSynConfig *c);
void func_00014d70(AlGlobalsLike *g, MusSynConfig *c) {
    if (D_800A9E50 == ((void *)0)) {
        D_800A9E50 = g;
        if (D_800A9E54 == ((void *)0)) {
            func_000154c0(c);
            D_800A9E54 = D_800E8298 = D_800A9E50;
            func_00014dd8(c);
        }
    }
}
void func_00014dd8(MusSynConfig *c);
void func_000154c0(MusSynConfig *p);
