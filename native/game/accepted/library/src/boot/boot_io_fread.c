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
s32 boot_io_fread(u8 *dst, s32 size, s32 n, ArcFile *f);
s32 boot_io_fread(u8 *dst, s32 size, s32 n, ArcFile *f) {
    s32 i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < size; j++) {
            if (f->pos >= f->size) {
                return i;
            }
            *dst++ = f->buf[f->pos++];
        }
    }
    return i;
}
