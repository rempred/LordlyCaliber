typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
typedef struct {
    void *data;
    s16 h4;
} Grid29;
typedef struct {
    u8 unk0[8];
    s16 h8;
    s16 hA;
} Pair29;
typedef struct {
    u8 unk0000[0x168];
    void *p168[0x38];
    Pair29 *p248[0x5BF];
    void *p1944[0x1C];
    u8 unk19B4[0x94];
    Grid29 *grid;
    u8 unk1A4C[0x168];
    void *p1BB4[20];
} Battle29;
extern Battle29 *D_8022A974;
void *func_000016C4(void *p);
typedef struct {
    s16 period;
    s16 count;
    s16 total;
    s16 pad;
    f32 delta;
} Drift29;
void func_0029A6B4(void) {
    s32 i;
    Drift29 *d;
    u8 *u;
    for (i = 0; i < 0x1C; i++) {
        d = (*(Drift29 * *)((u8 *)(D_8022A974) + (0x1A50 + i * 4)));
        if (d == ((void *)0)) {
            continue;
        }
        if (++d->count == d->total) {
            func_000016C4(d);
            (*(Drift29 * *)((u8 *)(D_8022A974) + (0x1A50 + i * 4))) = ((void *)0);
        } else {
            u = (*(u8 * *)((u8 *)(D_8022A974) + (0x18 + i * 4)));
            if (d->period == -1 || d->period - 1 == d->count % d->period) {
                (*(f32 *)((u8 *)(u) + (0x120))) += d->delta;
                if (!(u[0x145] & 4)) {
                    (*(f32 *)((u8 *)(u) + (0x128))) += d->delta;
                }
            }
        }
    }
}
