typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 *base;
    u8 *cur;
    s32 len;
    s32 count;
} MusHeap;
extern MusHeap D_800B1A70;
MusHeap *func_00015f24(void) {
    return &D_800B1A70;
}
