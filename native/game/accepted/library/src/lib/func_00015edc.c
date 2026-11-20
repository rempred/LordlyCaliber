typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 *base;
    u8 *cur;
    s32 len;
    s32 count;
} MusHeap;
extern MusHeap D_800B1A70;
void *alHeapDBAlloc(u8 *file, s32 line, void *hp, s32 num, s32 size);
void *func_00015edc(s32 size) {
    return alHeapDBAlloc(((void *)0), 0, &D_800B1A70, 1, size);
}
