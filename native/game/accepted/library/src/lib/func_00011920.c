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
s32 func_000140cc(MusSong *song);
void func_000122c8(s32 handle);
void func_000135a0(MusChannel *cp);
void func_00013664(MusChannel *cp);
extern u8 *(*D_800A98B0[])(MusChannel *cp, u8 *ptr);
s32 func_00011920(MusSong *song, s32 marker) {
    s32 handle = func_000140cc(song);
    s32 i;
    MusChannel *cp = D_800B180C;
    u8 *ptr;
    u8 cmd, v, d;
    u8 *q;
    s32 n;
    for (i = 0; i < D_800B1804; i++, cp++) {
        if (cp->handle != handle || cp->unk74 != song || cp->unk04 == 0) {
            continue;
        }
    next:
            ptr = (u8 *)cp->unk04;
            cmd = *ptr;
            if (cmd >= 0x80) {
                if (cmd == 0xAB && ptr[1] == marker) {
                    goto found;
                }
                cp->unk04 = (s32)D_800A98B0[cmd & 0x7F](cp, ptr + 1);
            } else {
                cp->unk04 = (s32)(ptr + 1);
                if (cp->velon) {
                    cp->unk04 = (s32)(ptr + 2);
                    v = ptr[1];
                    cp->unkBB = v;
                    if (v >= 0x80) {
                        cp->unkBB = v & 0x7F;
                        cp->velon = 0;
                        cp->unkD3 = cp->unkBB;
                    }
                } else {
                    cp->unkBB = cp->unkD3;
                }
                if (cp->length != 0 && cp->unkB7 == 0) {
                    cp->unk9A = cp->length;
                } else {
                    cp->unkB7 = 0;
                    q = (u8 *)cp->unk04;
                    cp->unk04 = (s32)(q + 1);
                    d = q[0];
                    if (d < 0x80) {
                        cp->unk9A = d;
                    } else {
                        cp->unk04 = (s32)(q + 2);
                        cp->unk9A = q[1] + ((d & 0x7F) << 8);
                    }
                }
                cp->unk0C += cp->unk9A << 8;
            }
        if (cp->unk04 != 0) {
            goto next;
        }
    found:
        cp->unk3C = cp->unk0C;
        if (cp->unk04 != 0) {
            ptr = (u8 *)cp->unk04 + 2;
            n = *ptr++;
            if (n >= 0x80) {
                n &= 0x7F;
                n <<= 8;
                n |= *ptr++;
            }
            cp->unkAA = 0;
            cp->unk9A = n;
            cp->unk04 = (s32)ptr;
            cp->unk0C -= n << 8;
        }
        cp->unk40 = cp->unk0C;
        if (cp->pvolume != ((void *)0)) {
            func_000135a0(cp);
        }
        if (cp->ppitchbend != ((void *)0)) {
            func_00013664(cp);
        }
    }
    func_000122c8(handle);
    return handle;
}
void func_000122c8(s32 handle);
void func_000135a0(MusChannel *cp);
void func_00013664(MusChannel *cp);
