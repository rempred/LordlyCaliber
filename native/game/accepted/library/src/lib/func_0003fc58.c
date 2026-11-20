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
void func_0003fc58(SoundChannel *ch) {
    if (!D_800EA924(ch, 2, 0, 0x4000)) {
        ch->command = 5;
        ch->param = 4;
        ch->pending = 1;
    }
}
