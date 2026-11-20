typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    s32 unk00;
    s32 request;
    s32 command;
    s32 fade;
    u8 unk10[0x14];
    s32 handle;
    s32 unk28;
    s32 saved[3];
    s32 unk38;
    s32 param;
    u8 unk40[0xC];
    s32 pending;
} SoundChannel;
s32 D_800EA924(SoundChannel *ch, s32 a, u16 b, s32 c);
extern SoundChannel D_800EB1F0;
extern SoundChannel D_800EB240;
extern SoundChannel D_800EB290;
static void func_0003FC40(SoundChannel *ch, s32 id, s32 fade);
void func_0003fb94(SoundChannel *ch, s32 id) {
    if (ch == &D_800EB1F0 || ch == &D_800EB240 || ch == ((void *)0)) {
        D_800EA924(ch, 1, id, 0x4000);
    } else if (ch == &D_800EB290) {
        D_800EA924(ch, 2, 0, 0x4000);
        D_800EA924(ch, 1, id, 0x4000);
    } else {
        ch->request = id + 1;
        ch->fade = 4;
        ch->pending = 1;
    }
}
static void func_0003FC40(SoundChannel *ch, s32 id, s32 fade) {
    ch->request = id + 1;
    ch->fade = fade;
    ch->pending = 1;
}
