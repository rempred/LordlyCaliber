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
s32 func_00011f08(s32 h);
extern s32 D_80164760;
static void func_0003FDFC(SoundChannel *ch, s32 param);
static void func_0003FE14(SoundChannel *ch, s32 param);
static void func_0003FE2C(SoundChannel *ch, s32 param);
static void func_0003FE44(SoundChannel *ch, s32 param);
static void func_0003FE5C(s32 v);
void func_0003fde0(SoundChannel *ch) {
    func_00011f08(ch->handle);
}
static void func_0003FDFC(SoundChannel *ch, s32 param) {
    ch->command = 11;
    ch->param = param;
    ch->pending = 1;
}
static void func_0003FE14(SoundChannel *ch, s32 param) {
    ch->command = 3;
    ch->param = param;
    ch->pending = 1;
}
static void func_0003FE2C(SoundChannel *ch, s32 param) {
    ch->command = 9;
    ch->param = param;
    ch->pending = 1;
}
static void func_0003FE44(SoundChannel *ch, s32 param) {
    ch->command = 12;
    ch->param = param;
    ch->pending = 1;
}
static void func_0003FE5C(s32 v) {
    D_80164760 = v;
}
