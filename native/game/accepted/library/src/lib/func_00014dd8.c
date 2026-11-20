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
typedef struct {
    MusLink node;
    void *vvoice;
    u8 pad0C[0x80];
} MusPVoice;
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
void *alHeapDBAlloc(u8 *file, s32 line, void *hp, s32 num, s32 size);
void alLink(void *ln, void *to);
void func_0001658c(void *pv, void *dma, void *hp);
s32 func_00017990(void);
s32 func_00016100(void);
s32 func_00015048(s16 idx, MusSynConfig *c, void *hp);
void func_00014dd8(MusSynConfig *c);
void func_00014dd8(MusSynConfig *c) {
    s32 i;
    MusPVoice *pv;
    MusPVoice *pvoices;
    MusParam *params;
    void *hp = c->heap;
    D_800A9E54->head = ((void *)0);
    D_800A9E54->numPVoices = c->maxPVoices;
    D_800A9E54->curSamples = 0;
    D_800A9E54->paramSamples = 0;
    D_800A9E54->outputRate = c->outputRate;
    D_800A9E54->maxOutSamples = 184;
    D_800A9E54->dma = c->dmaproc;
    D_800A9E54->sv_dramout = 0;
    D_800A9E54->sv_first = 1;
    D_800A9E54->unk34 = alHeapDBAlloc(((void *)0), 0, hp, 1, 0x44);
    D_800A9E54->unk34->sourceCount = 0;
    D_800A9E54->unk34->maxSources = c->maxPVoices;
    D_800A9E54->unk34->sources = alHeapDBAlloc(((void *)0), 0, hp, c->maxPVoices, 4);
    D_800A9E54->mainBus = alHeapDBAlloc(((void *)0), 0, hp, 1, 0x14);
    if (c->fxType != 0) {
        D_800A9E54->unk34->unk20 = (void *)func_00015048(0, c, hp);
        D_800A9E54->mainBus->handler = func_00017990;
    } else {
        D_800A9E54->mainBus->handler = func_00016100;
    }
    D_800A9E54->pFreeList.next = ((void *)0);
    D_800A9E54->pFreeList.prev = ((void *)0);
    D_800A9E54->pLameList.next = ((void *)0);
    D_800A9E54->pLameList.prev = ((void *)0);
    D_800A9E54->pAllocList.next = ((void *)0);
    D_800A9E54->pAllocList.prev = ((void *)0);
    pvoices = alHeapDBAlloc(((void *)0), 0, hp, c->maxPVoices, 0x8C);
    for (i = 0; i < c->maxPVoices; i++) {
        pv = &pvoices[i];
        alLink(pv, &D_800A9E54->pFreeList);
        pv->vvoice = ((void *)0);
        func_0001658c(pv, D_800A9E54->dma, hp);
        D_800A9E54->unk34->sources[D_800A9E54->unk34->sourceCount++] = pv;
    }
    params = alHeapDBAlloc(((void *)0), 0, hp, c->maxUpdates, 0x1C);
    D_800A9E54->paramList = ((void *)0);
    for (i = 0; i < c->maxUpdates; i++) {
        params[i].next = D_800A9E54->paramList;
        D_800A9E54->paramList = &params[i];
    }
    D_800A9E54->heap = hp;
}
s32 func_00015048(s16 idx, MusSynConfig *c, void *hp);
