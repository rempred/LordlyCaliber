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
extern u8 D_800EB0B0[];
extern s32 D_8022A8A8[];
void D_800EA838(SoundChannel *p, s32 gain);
void D_800EA884(SoundChannel *p, s32 gain);
void func_002831E0(s32 i, s32 gain) {
    if (i == 7) {
        D_800EA838((SoundChannel *)D_800EB0B0, gain);
    } else {
        D_800EA884((SoundChannel *)D_8022A8A8[i], gain);
    }
}
