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
extern s32 D_800B1804;
extern MusChannel *D_800B180C;
extern MusChannel *D_800B1810;
s32 func_00013c74(MusSong *song, s32 index) {
    s32 i, best, low;
    MusChannel *cp;
    if (index < 0) {
        cp = D_800B180C;
        for (i = 0; i < 4; i++, cp++) {
            if (cp->unk04 == 0) {
                return i;
            }
        }
    }
    for (i = 4, cp = D_800B1810; i < D_800B1804; i++, cp++) {
        if (cp->unk04 == 0) {
            return i;
        }
    }
    for (i = 4, cp = D_800B1810, low = 0x7FFFFFFF, best = 3; i < D_800B1804; i++, cp++) {
        if (cp->unk78 != 0 && cp->unk48 <= low) {
            low = cp->unk48;
            best = i;
        }
    }
    if (best >= 4) {
        return best;
    }
    for (i = 4, cp = D_800B1810; i < D_800B1804; i++) {
        if (cp->unk78 == 0 && cp->unk74 != song) {
            return i;
        }
    }
    for (i = 4, cp = D_800B1810; i < D_800B1804; i++, cp++) {
        if (cp->unk74 == song && song->channels[index] == (u32)cp->pbase) {
            return i;
        }
    }
    return (index % (D_800B1804 - 4)) + 4;
}
