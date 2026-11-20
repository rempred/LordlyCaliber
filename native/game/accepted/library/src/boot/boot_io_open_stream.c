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
extern ArcFile D_800AF340;
extern ArcFile D_800AF358;
ArcFile *boot_io_open_stream(void *name, u8 *mode, u8 *buf, s32 size);
ArcFile *boot_io_open_stream(void *name, u8 *mode, u8 *buf, s32 size) {
    if (mode[0] == 'r') {
        if (D_800AF340.open) {
            return ((void *)0);
        }
        D_800AF340.name = name;
        D_800AF340.buf = buf;
        D_800AF340.size = size;
        D_800AF340.pos = 0;
        return &D_800AF340;
    }
    if (D_800AF358.open) {
        return ((void *)0);
    }
    D_800AF358.name = name;
    D_800AF358.buf = buf;
    D_800AF358.size = size;
    D_800AF358.pos = 0;
    return &D_800AF358;
}
