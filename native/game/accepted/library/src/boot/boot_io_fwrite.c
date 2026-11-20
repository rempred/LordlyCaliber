typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 open;
    void *name;
    u8 *buf;
    s32 size;
    s32 pos;
    s32 unk14;
} ArcFile;
static void func_0000FA40(void);
s32 boot_io_fwrite(u8 *src, s32 size, s32 n, ArcFile *f) {
    s32 i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < size; j++) {
            if (f->pos >= f->size) {
                return i;
            }
            f->buf[f->pos++] = *src++;
        }
    }
    return i;
}
static void func_0000FA40(void) {
}
