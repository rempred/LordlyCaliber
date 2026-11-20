typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u8 unk00[0xC];
    void *fb;
} FrameMsg;
extern void *D_800C4BB8;
u8 D_8016CD3C(void);
void osViSwapBuffer(void *fb);
extern u8 osViModeNtscLan1[];
extern u8 osViModeMpalLan1[];
extern u8 D_800AB9B0[];
extern u8 D_800ABA50[];
void osViSetMode(void *m);
extern s32 osTvType;
void func_00025a10(u32 f);
typedef signed short s16;
typedef unsigned short u16;
extern u16 D_800C4C08;
extern u16 D_800E7D68;
extern u16 D_800C4A18;
extern u16 D_800E7A1C;
extern u16 D_800C4BCA;
extern u16 D_800C4AD8;
extern u8 D_800AEE72;
static void func_00003564(u8 relative, s16 a, s16 b, s16 c, s16 d, s16 e, s16 f);
void boot_mode_message_accumulator_update(FrameMsg *msg) {
    u8 s;
    D_800C4BB8 = msg->fb;
    s = D_8016CD3C();
    if (s == 2) {
        switch (osTvType) {
            case 1:
                osViSetMode(D_800AB9B0);
                break;
            case 2:
                osViSetMode(D_800ABA50);
                break;
        }
    } else if (s == 1) {
        switch (osTvType) {
            case 1:
                osViSetMode(&osViModeNtscLan1);
                break;
            case 2:
                osViSetMode(&osViModeMpalLan1);
                break;
        }
    }
    if (s != 0) {
        func_00025a10(0x5A);
    }
    osViSwapBuffer(msg->fb);
}
static void func_00003564(u8 relative, s16 a, s16 b, s16 c, s16 d, s16 e, s16 f) {
    if (!relative) {
        if (a >= 0) {
            D_800C4C08 = a;
        }
        if (b >= 0) {
            D_800E7D68 = b;
        }
        if (c >= 0) {
            D_800C4A18 = c;
        }
        if (d >= 0) {
            D_800E7A1C = d;
        }
        if (e >= 0) {
            D_800C4BCA = e;
        }
        if (f >= 0) {
            D_800C4AD8 = f;
        }
    } else {
        D_800C4C08 += a;
        D_800E7D68 += b;
        D_800C4A18 += c;
        D_800E7A1C += d;
        D_800C4BCA += e;
        D_800C4AD8 += f;
    }
    D_800AEE72 = 2;
}
