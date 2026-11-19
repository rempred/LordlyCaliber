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
               s32 unk00;
               s32 unk04;
               void *pdata;
               s32 unk0C;
               s32 unk10;
               s32 unk14;
               s32 unk18;
               s32 unk1C;
               f32 vib_amount;
               f32 unk24;
               f32 unk28;
               f32 unk2C;
               f32 unk30;
               u8 *ppitchbend;
               u8 *pvolume;
               s32 unk3C;
               s32 unk40;
               s32 handle;
               s32 unk48;
               f32 unk4C;
               f32 unk50;
               s32 unk54;
               f32 unk58;
               f32 unk5C;
               f32 unk60;
               s32 unk64;
               f32 unk68;
               f32 unk6C;
               f32 unk70;
               void *unk74;
               void *unk78;
               s32 unk7C;
               u8 *pbase;
               s32 unk84;
               u8 *ppitchbendbase;
               u8 *pvolumebase;
               f32 unk90;
               s32 unk94;
               s16 unk98;
               u16 unk9A;
               u16 unk9C;
               s16 unk9E;
               u16 unkA0;
               u16 unkA2;
               u16 unkA4;
               u16 unkA6;
               u16 unkA8;
               u16 unkAA;
               u16 length;
               u16 unkAE;
               s16 unkB0;
               u16 unkB2;
               u16 unkB4;
               u8 vib_delay;
               u8 unkB7;
               u8 porta;
               u8 unkB9;
               u8 unkBA;
               u8 unkBB;
               u8 unkBC;
               u8 unkBD;
               u8 unkBE;
               u8 unkBF;
               u8 unkC0;
               u8 unkC1;
               u8 unkC2;
               u8 unkC3;
               u8 unkC4;
               u8 unkC5;
               u8 unkC6;
               u8 unkC7;
               u8 unkC8;
               u8 unkC9;
               u8 unkCA;
               u8 unkCB;
               u8 unkCC;
               u8 unkCD;
               u8 wobble;
               u8 unkCF;
               u8 unkD0;
               s8 unkD1;
               u8 velon;
               u8 unkD3;
               u8 unkD4;
               u8 vib_speed;
               u8 env;
               u8 trigger;
               u8 unkD8;
               u8 sweep_timer;
               u8 unkDA;
               u8 depth;
               f32 vib_precalc;
               u8 *loopptr[4];
               u8 *loopvol[4];
                u8 *looppb[4];
                u16 loopA2[4];
                u16 loopA4[4];
                u8 loopcount[4];
                u8 loopBC[4];
                f32 loop70[4];
                u8 pad138[0x4];
} MusChannel;
typedef struct {
               u32 base_u;
               s32 len;
               u8 type;
               u8 flags;
               u32 loop;
               u32 book;
} MusWave;
typedef struct {
               u8 pad00[0x10];
               s32 flags;
               u8 pad14[0xC];
               s32 count;
               u8 *basenote;
               u8 *detune;
               MusWave **waves;
} MusBank;
typedef struct {
               s32 unk00;
               s32 count;
               u8 pad08[4];
               s32 flags;
               void *ptrbank;
               u32 unk14;
               struct {
        u32 ptr;
        u32 unk4;
    } entries[1];
} MusFxBank;
typedef struct {
               u16 wave;
               u16 envelope;
               u8 pan;
               u8 note;
} MusDrum;
typedef struct {
               u8 pad00[4];
               s32 count;
               s32 unk08;
               u32 *channels;
               u32 *volumes;
               u32 *pitchbends;
               u8 *envelopes;
               MusDrum *drums;
               u16 *wavetable;
               u32 master;
               s32 relocated;
} MusSong;
typedef struct {
               u8 type;
               s32 value;
} MusCommand;
extern MusCommand *D_800B1850;
extern s32 D_800B184C;
extern s32 D_800B1848;
extern s32 D_800B1844;
void *__MusIntMemMalloc(s32 size);
void func_80083F38(s32 handle, s32 a, s32 b);
void func_80085B5C(void *dst, void *src, s32 n);
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
void func_800850C0(MusSynConfig *p);
typedef struct MusLink {
    struct MusLink *next;
    struct MusLink *prev;
} MusLink;
typedef struct {
               u32 input;
               u32 output;
               s16 ffcoef;
               s16 fbcoef;
               s16 gain;
               f32 rsinc;
               f32 rsval;
               s32 rsdelta;
               f32 rsgain;
               struct {
        s16 fc;
        u8 pad02[0x26];
        void *fstate;
    } *lp;
               struct {
        u8 pad00[0x14];
        void *state;
        u8 pad18[8];
        f32 delta;
        s32 first;
    } *rs;
} MusDelay;
typedef struct {
               u8 filter[0x14];
               s16 *base;
               s16 *input;
               u32 length;
               MusDelay *delay;
               u8 section_count;
               void *paramHdl;
} MusFx;
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
extern AlGlobalsLike *alGlobals_800A9E54;
extern AlGlobalsLike *n_alGlobals;
extern AlGlobalsLike *D_800E8298;
void func_80084C98(void **out, MusSynConfig *c, void *hp);
extern u16 D_800B181C;
extern u16 D_800B181E;
extern s32 D_800B1818;
void func_80088410(void *voice, s16 vol, s32 time);
void func_80088240(void *voice, u8 fxmix);
void func_80082DC4(MusChannel *cp);
void __MusIntInitSweep(MusChannel *cp);
void func_800882E0(void *voice, u8 pan);
void func_80083A3C(void *table, void *base, s32 n);
void *alHeapDBAlloc(u8 *file, s32 line, void *hp, s32 num, s32 size);
s32 alFxParamHdl(void *fx, s32 id, void *param);
typedef struct {
               void *unkA8;
               void *unkAC;
               void *unkB0;
               void *unkB4;
               void *unkB8;
               void *unkBC;
} MusFxState;
extern MusFxState D_800E82A8;
s32 func_800850D8(void);
s32 func_8008511C(void);
void osWritebackDCacheAll(void);
extern void *D_800B1828;
extern MusBank *D_800B182C;
extern s32 D_800B1830;
extern s32 D_800B1834;
extern MusFxBank *D_800B1838;
extern MusFxBank *D_800B183C;
extern s32 D_800B1840;
extern s32 D_800B1804;
extern MusChannel *D_800B180C;
s32 func_80083B24(MusFxBank *bank, s32 number, s32 volume, s32 pan, s32 priority);
s32 func_80083A74(MusChannel *cp, MusFxBank *bank, s32 number, s32 volume, s32 pan, s32 priority);
extern MusChannel *D_800B1810;
extern s32 D_800B1814;
f32 func_80083378(f32 pitch);
void func_80088380(void *voice, f32 pitch);
extern s32 D_800A9B8C;
extern s32 D_800A99D8[];
s32 func_80083CCC(MusSong *song);
void MusHandleUnPause(s32 handle);
void MusSetMasterVolume(u32 flags, s32 volume);
void func_80081DB4(MusBank *bank, u8 *wbank);
void func_80081F7C(MusFxBank *bank);
void func_80082130(s32 n);
s32 func_800822AC(void *player);
void func_80082AE8(MusChannel *cp, s32 voice);
void func_80082B64(MusChannel *cp, s32 voice);
void func_80082CB8(MusChannel *cp, s32 voice, f32 offset);
void func_80082E34(MusChannel *cp);
void func_8008302C(MusChannel *cp);
f32 func_800830E4(MusChannel *cp);
f32 func_8008313C(MusChannel *cp);
void func_8008259C(MusChannel *cp, s32 voice);
void func_800831A0(MusChannel *cp);
void func_80083264(MusChannel *cp);
typedef struct {
               u32 control_flag;
               s32 channels;
               void *sched;
               s32 thread_priority;
               u8 *heap;
               s32 heap_length;
               MusBank *ptr;
               u8 *wbk;
               MusFxBank *default_fxbank;
               s32 fifo_length;
               s32 syn_updates;
               s32 syn_output_rate;
               s32 syn_rsp_cmds;
               s32 syn_retraceCount;
               s32 syn_num_dma_bufs;
               s32 syn_dma_buf_size;
               s32 diskrom_handle;
} MusConfig;
typedef struct {
    s16 priority;
    s16 fxBus;
    u8 unityPitch;
} MusVoiceConfig;
typedef struct MusPlayer {
               struct MusPlayer *next;
               void *clientData;
               void (*handler)(void *);
} MusPlayer;
extern s32 osTvType;
extern s32 D_800C4B48;
extern u32 D_800E7A14;
extern MusPlayer D_800B17F0;
extern u8 *(*D_800A98B0[])(MusChannel *cp, u8 *ptr);
void func_80085A90(u8 *heap, s32 length);
void func_80085620(void *sched);
void func_800857D0(MusConfig *config, s32 rate, s32 n);
void func_80088000(MusPlayer *player);
void func_80088050(void *voice, MusVoiceConfig *vc);
void func_80085B0C(void);
void func_8008350C(MusBank *pbank, u8 *wbank);
s32 func_80082214(MusCommand *cmd);
s32 ChangeCustomEffect(s32 fx);
s32 func_800836A4(s32 range);
void func_80083738(MusChannel *cp);
extern s32 D_800B1820;
extern s32 D_800B1824;
extern u8 *D_800B1808;
void func_80088550(void *voice);
void func_800884C0(void *voice, void *wave);
extern s32 D_800A9B60;
void func_80084DE0(s32 *param);
void alLink(void *ln, void *to);
void alN_PVoiceNew(void *pv, void *dma, void *hp);
void init_lpfilter(void *lp);
s32 func_80087590(void);
s32 func_80085D00(void);
s32 func_80084C48(s16 idx, MusSynConfig *c, void *hp);
void alFxParam(void *fx, s32 id, void *param);
extern inline void __MusIntSetEnvelope(MusChannel *cp, u8 *env) {
    u8 v, t;
    v = *env++;
    if (v == 0) {
        v = 1;
    }
    cp->unkBF = v;
    cp->unk64 = 0x400 / v;
    cp->unkC0 = *env++;
    v = *env++;
    cp->unkC6 = v;
    t = *env++;
    cp->unk58 = (1.0 / (f32)v) * (f32)(t - cp->unkC0);
    cp->unkC1 = t;
    v = *env++;
    cp->unkC7 = v;
    t = *env++;
    cp->unk5C = (1.0 / (f32)v) * (f32)(t - cp->unkC1);
    cp->unkC2 = t;
    v = *env++;
    cp->unkC8 = v;
    cp->unk60 = 1.0 / (f32)v;
}
u8 *func_800810F0(MusChannel *cp, u8 *ptr);
void func_8008123C(MusConfig *config);
void MusSetMasterVolume(u32 flags, s32 volume);
s32 MusStartSong(void *addr);
s32 func_80081520(MusSong *song, s32 marker);
s32 func_80081770(s32 number);
s32 func_800817F0(s32 number, s32 volume, s32 pan, s32 restartflag, s32 priority);
void func_80081908(u32 flags, s32 speed);
s32 func_800819D4(u32 flags);
s32 func_80081B60(s32 handle, u16 value);
s32 func_80081BC0(s32 handle, u16 value);
s32 func_80081C30(s32 handle, f32 value);
s32 func_80081CA0(s32 handle, s32 tempo);
s32 func_80081D2C(s32 handle, s32 pan);
void func_80081DB4(MusBank *bank, u8 *wbank);
MusBank *func_80081DF0(MusBank *bank);
void MusPtrBankSetCurrent(MusBank *bank);
MusBank *func_80081E3C(void);
s32 func_80081E48(s32 handle);
void MusHandlePause(s32 handle);
void MusHandleUnPause(s32 handle);
extern inline s32 MusSetFxType(s32 type) {
    MusCommand cmd;
    cmd.type = 2;
    cmd.value = type;
    D_800B1834 = type;
    return func_80082214(&cmd);
}
void func_80081F20(s32 type);
s32 MusFxBankNumberOfEffects(MusFxBank *bank);
void func_80082008(MusFxBank *a);
void func_80082014(MusFxBank *a);
MusFxBank *func_80082020(void);
void MusFxBankSetPtrBank(MusFxBank *bank, void *ptrbank);
void *MusFxBankGetPtrBank(MusFxBank *bank);
void func_8008203C(s32 a);
s32 func_800820AC(s32 handle);
void func_80082124(s32 a);
void func_80082130(s32 n);
void func_80082194(MusCommand *cmd);
s32 func_80082214(MusCommand *cmd);
s32 func_800822AC(void *player);
void func_8008259C(MusChannel *cp, s32 voice) {
    u8 *ptr;
    s32 note;
    u8 v, t, rv;
    u16 idx;
    MusBank *pbank;
    ptr = (u8 *)cp->unk04;
    if (ptr != ((void *)0)) {
    next:
        t = *ptr;
        if (t >= 0x80) {
            ptr = D_800A98B0[t & 0x7F](cp, ptr + 1);
            if (ptr != ((void *)0)) {
                goto next;
            }
        }
    }
    cp->unk04 = (s32)ptr;
    if (ptr != ((void *)0)) {
        cp->unk4C = cp->unk50;
        note = *(*(u8 **)&(cp)->unk04)++;
        if (cp->velon) {
            v = *(*(u8 **)&(cp)->unk04)++;
            if (v >= 0x80) {
                v &= 0x7F;
                cp->velon = 0;
                cp->unkD3 = v;
            }
            if (note != 0x60) {
                cp->unkBB = v;
            }
        } else {
            cp->unkBB = cp->unkD3;
        }
        if (cp->length != 0) {
            if (cp->unkB7 != 0) {
                cp->unkB7 = 0;
                t = *(*(u8 **)&(cp)->unk04)++;
                if (t < 0x80) {
                    cp->unk9A = t;
                } else {
                    cp->unk9A = *(*(u8 **)&(cp)->unk04)++ + ((t & 0x7F) << 8);
                }
            } else {
                cp->unk9A = cp->length;
            }
        } else {
            t = *(*(u8 **)&(cp)->unk04)++;
            if (t < 0x80) {
                cp->unk9A = t;
            } else {
                cp->unk9A = *(*(u8 **)&(cp)->unk04)++ + ((t & 0x7F) << 8);
            }
        }
        cp->unk40 = cp->unk3C;
        cp->unk3C = cp->unk3C + (cp->unk9A << 8);
        cp->unkAA = 0;
        cp->unkD1 = 0;
        cp->unkD0 = cp->unkCF;
        if (cp->unk74 != ((void *)0) && cp->unk84 == 0 &&
            ((MusSong *)cp->unk74)->wavetable[cp->unkAE] == 0xFFFF) {
            note = 0x60;
        }
        if (note != 0x60) {
            pbank = (MusBank *)cp->unk7C;
            if (cp->unk84 != 0) {
                cp->unkAE = ((MusDrum *)cp->unk84)[note].wave;
                cp->unkBD = ((MusDrum *)cp->unk84)[note].pan >> 1;
                __MusIntSetEnvelope(cp, ((MusSong *)cp->unk74)->envelopes + ((MusDrum *)cp->unk84)[note].envelope * 7);
                note = ((MusDrum *)cp->unk84)[note].note;
            }
            if (cp->env == 0) {
                func_80082DC4(cp);
            }
            if (cp->unkD4 != 0) {
                __MusIntInitSweep(cp);
            }
            idx = cp->unkAE;
            if (cp->unk74 != ((void *)0)) {
                idx = ((MusSong *)cp->unk74)->wavetable[idx];
            } else {
                idx = ((u16 *)((MusFxBank *)cp->unk78)->unk14)[idx];
            }
            if (cp->trigger == 0) {
                cp->pdata = pbank->waves[idx];
                if (cp->unkC9 != 0 && cp->unkA0 != 0) {
                    cp->unkA0 = 0;
                    func_80088410(D_800B1808 + voice * 0x1C, 0, D_800B1818);
                } else {
                    func_80082AE8(cp, voice);
                }
            }
            cp->unk2C = (f32)note + ((f32 *)pbank->detune)[idx];
            t = cp->unkB9 * (1 - cp->unkBA);
            cp->unk2C = cp->unk2C + (f32)((t & 0x80) ? t - 0x100 : t);
            rv = cp->unkCA;
            if (rv != cp->unkCC) {
                cp->unkCC = rv;
                func_80088240(D_800B1808 + voice * 0x1C, cp->unkCB + (((0x80 - cp->unkCB) * rv) >> 7));
            }
        } else if (cp->unkC3 < 4) {
            cp->unkC3 = 4;
            cp->unkC5 = 1;
            cp->unk54 = cp->unk0C;
            cp->unkCD = cp->unkC4;
        }
    } else if (cp->unkC9 != 0) {
        cp->unkC9 = 0;
        func_80088410(D_800B1808 + voice * 0x1C, 0, D_800B1818);
        func_80088550(D_800B1808 + voice * 0x1C);
    }
}
void func_80082AE8(MusChannel *cp, s32 voice);
void func_80082B64(MusChannel *cp, s32 voice);
void func_80082CB8(MusChannel *cp, s32 voice, f32 offset);
void func_80082DC4(MusChannel *cp);
void func_80082E34(MusChannel *cp);
void __MusIntInitSweep(MusChannel *cp);
void func_8008302C(MusChannel *cp);
f32 func_800830E4(MusChannel *cp);
f32 sinf(f32);
f32 func_8008313C(MusChannel *cp);
void func_800831A0(MusChannel *cp);
void func_80083264(MusChannel *cp);
s32 func_800836A4(s32 range);
s32 func_80083874(MusSong *song, s32 index);
extern inline void func_80083A3C(void *table, void *base, s32 n) {
    u32 *p = table;
    u32 b = (u32)base;
    s32 i;
    for (i = 0; i < n; i++, p++) {
        if (*p != 0) {
            *p += b;
        }
    }
}
extern inline s32 func_80083A74(MusChannel *cp, MusFxBank *bank, s32 number, s32 volume, s32 pan, s32 priority) {
    func_80083738(cp);
    cp->unkA6 = number;
    cp->unk78 = bank;
    cp->unk9E = volume;
    cp->unkB0 = pan;
    cp->handle = D_800B1820++;
    cp->unk48 = priority;
    if (bank->ptrbank != ((void *)0)) {
        cp->unk7C = (s32)bank->ptrbank;
    }
    cp->unk04 = (s32)(cp->pbase = (u8 *)bank->entries[number].ptr);
    return cp->handle;
}
s32 func_80083B24(MusFxBank *bank, s32 number, s32 volume, s32 pan, s32 priority);
void func_80083F38(s32 handle, s32 and, s32 or);
u8 *Fstop(MusChannel *cp, u8 *ptr);
u8 *func_80083FAC(MusChannel *cp, u8 *ptr);
u8 *func_80083FDC(MusChannel *cp, u8 *ptr);
u8 *Fportoff(MusChannel *cp, u8 *ptr);
u8 *func_80084008(MusChannel *cp, u8 *ptr);
u8 *func_80084130(MusChannel *cp, u8 *ptr);
u8 *func_80084214(MusChannel *cp, u8 *ptr);
u8 *func_80084228(MusChannel *cp, u8 *ptr);
u8 *func_8008424C(MusChannel *cp, u8 *ptr);
u8 *Fviboff(MusChannel *cp, u8 *ptr);
u8 *func_80084340(MusChannel *cp, u8 *ptr);
u8 *func_80084370(MusChannel *cp, u8 *ptr);
u8 *func_80084380(MusChannel *cp, u8 *ptr);
u8 *func_80084390(MusChannel *cp, u8 *ptr);
u8 *func_800843A0(MusChannel *cp, u8 *ptr);
u8 *func_800843F8(MusChannel *cp, u8 *ptr);
u8 *Fenvon(MusChannel *cp, u8 *ptr);
u8 *func_80084414(MusChannel *cp, u8 *ptr);
u8 *Ftron(MusChannel *cp, u8 *ptr);
u8 *func_80084430(MusChannel *cp, u8 *ptr);
u8 *func_8008449C(MusChannel *cp, u8 *ptr);
u8 *func_80084538(MusChannel *cp, u8 *ptr);
u8 *Fwobbleoff(MusChannel *cp, u8 *ptr);
u8 *func_8008456C(MusChannel *cp, u8 *ptr);
u8 *Fveloff(MusChannel *cp, u8 *ptr);
u8 *func_80084588(MusChannel *cp, u8 *ptr);
u8 *func_8008459C(MusChannel *cp, u8 *ptr);
u8 *Fstereo(MusChannel *cp, u8 *ptr);
u8 *func_800845B8(MusChannel *cp, u8 *ptr);
u8 *Fdrumsoff(MusChannel *cp, u8 *ptr);
u8 *Fprint(MusChannel *cp, u8 *ptr);
u8 *func_80084684(MusChannel *cp, u8 *ptr);
u8 *func_80084694(MusChannel *cp, u8 *ptr);
u8 *func_800846E4(MusChannel *cp, u8 *ptr);
u8 *func_80084734(MusChannel *cp, u8 *ptr);
u8 *func_80084784(MusChannel *cp, u8 *ptr);
u8 *func_80084794(MusChannel *cp, u8 *ptr);
u8 *func_80084864(MusChannel *cp, u8 *ptr);
extern const f64 D_800AE4E8;
u8 *func_800848A0(MusChannel *cp, u8 *ptr);
u8 *Fchangefx(MusChannel *cp, u8 *ptr);
u8 *func_800848F4(MusChannel *cp, u8 *ptr);
u8 *Flength0(MusChannel *cp, u8 *ptr);
void func_800849D8(MusSynConfig *c);
void func_80084970(AlGlobalsLike *g, MusSynConfig *c);
void func_800849D8(MusSynConfig *c);
s32 func_80084C48(s16 idx, MusSynConfig *c, void *hp);
void func_80084C98(void **out, MusSynConfig *c, void *hp);
void func_80084DE0(s32 *param);
void func_800850C0(MusSynConfig *p);
extern s32 *D_800A9B64[];
s32 func_800850D8(void);
s32 func_8008511C(void);
s32 ChangeCustomEffect(s32 fx);
