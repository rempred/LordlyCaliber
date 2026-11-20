typedef unsigned char u8;
typedef signed short s16;
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
void func_00015098(void **out, MusSynConfig *c, void *hp);
s32 func_00015048(s16 idx, MusSynConfig *c, void *hp);
s32 func_00015048(s16 idx, MusSynConfig *c, void *hp) {
    func_00015098((void **)&D_800A9E54->unk34->arr[idx], c, hp);
    return D_800A9E54->unk34->arr[idx];
}
void func_00015098(void **out, MusSynConfig *c, void *hp);
