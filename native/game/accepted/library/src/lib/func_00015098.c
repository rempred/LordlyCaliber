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
void func_00015098(void **out, MusSynConfig *c, void *hp);
void *alHeapDBAlloc(u8 *file, s32 line, void *hp, s32 num, s32 size);
s32 func_0001f2b8(void *fx, s32 id, void *param);
typedef struct {
    void *unkA8;
    void *unkAC;
    void *unkB0;
    void *unkB4;
    void *unkB8;
    void *unkBC;
} MusFxState;
extern MusFxState D_800E82A8;
s32 func_000154d8(void);
s32 func_0001551c(void);
void func_000151e0(s32 *param);
void func_00015098(void **out, MusSynConfig *c, void *hp) {
    s32 *fx = alHeapDBAlloc(((void *)0), 0, hp, 1, 0x2C);
    *out = fx;
    fx[10] = (s32)func_0001f2b8;
    D_800E82A8.unkA8 = alHeapDBAlloc(((void *)0), 0, hp, func_000154d8(), 0x28);
    D_800E82A8.unkAC = alHeapDBAlloc(((void *)0), 0, hp, func_0001551c(), 2);
    D_800E82A8.unkB0 = alHeapDBAlloc(((void *)0), 0, hp, 1, 0x34);
    D_800E82A8.unkB4 = alHeapDBAlloc(((void *)0), 0, hp, 1, 0x20);
    D_800E82A8.unkB8 = alHeapDBAlloc(((void *)0), 0, hp, 1, 0x30);
    D_800E82A8.unkBC = alHeapDBAlloc(((void *)0), 0, hp, 1, 8);
    func_000151e0(c->params);
}
void func_000151e0(s32 *param);
s32 func_000154d8(void);
s32 func_0001551c(void);
