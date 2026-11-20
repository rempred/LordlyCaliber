typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 type;
    s32 value;
} MusCommand;
extern s32 D_800B1830;
extern s32 D_800B1834;
s32 func_00012614(MusCommand *cmd);
extern inline s32 func_000122f0(s32 type) {
    MusCommand cmd;
    cmd.type = 2;
    cmd.value = type;
    D_800B1834 = type;
    return func_00012614(&cmd);
}
void func_00012320(s32 type) {
    s32 ok = 1;
    if (type == 0) {
        ok = func_000122f0(D_800B1834);
    }
    if (ok) {
        D_800B1830 = type;
    }
}
s32 func_00012614(MusCommand *cmd);
