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
extern s32 D_800A99D8[];
void func_000154c0(MusSynConfig *p) {
    p->fxType = 6;
    p->params = D_800A99D8;
}
