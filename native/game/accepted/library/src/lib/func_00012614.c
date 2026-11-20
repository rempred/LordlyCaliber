typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 type;
    s32 value;
} MusCommand;
extern MusCommand *D_800B1850;
extern s32 D_800B184C;
extern s32 D_800B1848;
extern s32 D_800B1844;
void memmove(void *dst, void *src, s32 n);
s32 func_00012614(MusCommand *cmd);
s32 func_00012614(MusCommand *cmd) {
    s32 next = (D_800B1848 + 1) % D_800B184C;
    if (next == D_800B1844) {
        return 0;
    }
    memmove(D_800B1850 + D_800B1848, cmd, 8);
    D_800B1848 = next;
    return 1;
}
