typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
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
    u8 type;
    s32 value;
} MusCommand;
extern MusCommand *D_800B1850;
extern s32 D_800B184C;
extern s32 D_800B1848;
extern s32 D_800B1844;
extern s32 D_800B1818;
extern s32 D_800B1804;
extern MusChannel *D_800B180C;
s32 func_000126ac(void *player);
void func_00012ee8(MusChannel *cp, s32 voice);
void func_00012f64(MusChannel *cp, s32 voice);
void func_000130b8(MusChannel *cp, s32 voice, f32 offset);
void func_00013234(MusChannel *cp);
void func_0001342c(MusChannel *cp);
f32 func_000134e4(MusChannel *cp);
f32 func_0001353c(MusChannel *cp);
void func_0001299c(MusChannel *cp, s32 voice);
void func_000135a0(MusChannel *cp);
void func_00013664(MusChannel *cp);
extern u8 *D_800B1808;
void func_00018950(void *voice);
void func_00012594(MusCommand *cmd);
s32 func_000126ac(void *player) {
    s32 i;
    MusChannel *cp;
    f32 pitch;
    while (D_800B1844 != D_800B1848) {
        func_00012594(&D_800B1850[D_800B1844]);
        D_800B1844++;
        if (D_800B1844 == D_800B184C) {
            D_800B1844 = 0;
        }
    }
    for (i = -4, cp = D_800B180C; i < D_800B1804 - 4; i++, cp++) {
        if (cp->unk04 == 0) {
            continue;
        }
        if (cp->unk00 & 1) {
            continue;
        }
        if (cp->pdata != ((void *)0)) {
            func_00012ee8(cp, i);
        }
        cp->unk0C += cp->unk9C;
        if (cp->unk9A != 0x7FFF) {
            while (cp->unk3C - cp->unk0C < 0) {
                if (cp->unk04 == 0) {
                    goto next;
                }
                func_0001299c(cp, i);
            }
            if (cp->unk04 == 0) {
                continue;
            }
        }
        if (cp->pvolume != ((void *)0) && cp->unk14 - cp->unk0C < 0) {
            func_000135a0(cp);
        }
        if (cp->ppitchbend != ((void *)0) && cp->unk18 - cp->unk0C < 0) {
            func_00013664(cp);
        }
        if (cp->unk10 != -1 && --cp->unk10 == -1) {
            cp->pvolume = ((void *)0);
            cp->ppitchbend = ((void *)0);
            cp->unk74 = ((void *)0);
            cp->unk78 = ((void *)0);
            cp->handle = 0;
            cp->pdata = ((void *)0);
            cp->unk04 = 0;
            if (cp->unkC9 != 0) {
                cp->unkC9 = 0;
                func_00018950(D_800B1808 + i * 0x1C);
            }
        }
        if (cp->unkC9 != 0) {
            if (cp->unkC3 != 0) {
                func_00013234(cp);
            }
            if (cp->unkD4 != 0 && cp->unk94 - cp->unk0C < 0) {
                func_0001342c(cp);
            }
            pitch = cp->unk30;
            if (cp->vib_speed != 0) {
                pitch += func_0001353c(cp);
            }
            if (cp->wobble != 0) {
                pitch += func_000134e4(cp);
            }
            if (cp->pdata == ((void *)0)) {
                func_000130b8(cp, i, pitch);
                func_00012f64(cp, i);
            }
        }
        cp->unkAA = (u32)(cp->unk0C - cp->unk40) >> 8;
    next:;
    }
    return D_800B1818;
}
void func_0001299c(MusChannel *cp, s32 voice);
void func_00012ee8(MusChannel *cp, s32 voice);
void func_00012f64(MusChannel *cp, s32 voice);
void func_000130b8(MusChannel *cp, s32 voice, f32 offset);
void func_00013234(MusChannel *cp);
void func_0001342c(MusChannel *cp);
f32 func_000134e4(MusChannel *cp);
f32 func_0001353c(MusChannel *cp);
void func_000135a0(MusChannel *cp);
void func_00013664(MusChannel *cp);
