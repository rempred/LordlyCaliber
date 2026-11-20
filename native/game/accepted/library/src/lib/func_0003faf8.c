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
extern u8 D_80104710[];
s32 func_00011f08(s32 h);
void func_00011e54(s32 h, s32 a);
void func_000121f0(void *p);
s32 func_00011b70(s32 a);
void func_0003faf8(SoundChannel *ch);
void func_0003faf8(SoundChannel *ch) {
    if ((*(s32 *)((u8 *)(ch) + (0x18))) != 0) {
        if (func_00011f08(ch->handle)) {
            func_00011e54(ch->handle, 0);
        }
    } else if (ch->unk28 != -1 && func_00011f08(ch->unk28)) {
        func_00011e54(ch->unk28, 0);
    }
    ch->unk28 = ch->handle;
    func_000121f0(D_80104710);
    ch->handle = func_00011b70(ch->saved[0] - 1);
    (*(s32 *)((u8 *)(ch) + (0x20))) = ch->saved[0];
}
