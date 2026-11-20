typedef unsigned char u8;
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
extern SoundChannel D_800EB0B0[];
void func_00011d08(s32 a, s32 b);
void func_0003fca4(void) {
    D_800EB0B0[0].saved[0] = 0;
    D_800EB0B0[1].saved[0] = 0;
    D_800EB0B0[2].saved[0] = 0;
    D_800EB0B0[3].saved[0] = 0;
    D_800EB0B0[4].saved[0] = 0;
    D_800EB0B0[5].saved[0] = 0;
    D_800EB0B0[6].saved[0] = 0;
    func_00011d08(3, 0);
}
