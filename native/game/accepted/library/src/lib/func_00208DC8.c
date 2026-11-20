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

extern const unsigned char g_func_0015DF10_match[];
extern const unsigned char g_func_0015DF10_default[];
extern unsigned char D_800EB0B0[];
extern unsigned char D_800EB100[];
extern unsigned char D_800EB150[];
extern unsigned char D_800EB1A0[];
extern unsigned short D_800E810E;
extern void func_800EA604(SoundChannel *channel, int request);

void func_00208DC8(void *arg0, int arg1)
{
    if (arg0 == g_func_0015DF10_match) {
        *(int *)0x801D06C4 = arg1;
    } else if (arg0 == g_func_0015DF10_default) {
        *(int *)0x801D06C8 = arg1;
    } else if (arg0 == 0) {
        if (D_800E810E != 12) {
            func_800EA604(0, arg1);
        }
    } else if (arg0 == D_800EB0B0) {
        *(int *)0x801D06B0 = arg1;
    } else if (arg0 == D_800EB100) {
        *(int *)0x801D06B4 = arg1;
    } else if (arg0 == D_800EB150) {
        *(int *)0x801D06B8 = arg1;
    } else if (arg0 == D_800EB1A0) {
        *(int *)0x801D06BC = arg1;
    } else {
        *(int *)0x801D06C0 = arg1;
    }
}
