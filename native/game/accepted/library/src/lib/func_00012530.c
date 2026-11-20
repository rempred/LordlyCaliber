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
void *func_00015edc(s32 size);
void func_00012530(s32 n);
void func_00012530(s32 n) {
    if (n < 0x40) {
        n = 0x40;
    } else if (n > 0x400) {
        n = 0x400;
    }
    D_800B1850 = func_00015edc(n * 8);
    D_800B184C = n;
    D_800B1848 = 0;
    D_800B1844 = 0;
}
