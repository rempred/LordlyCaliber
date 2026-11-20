typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
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
void *func_00001330(s32 size);
void *func_000016C4(void *p);
void func_0029B388(s32 a, s32 b);
void func_0029B5A4(void) {
    Grid29 *g;
    if (D_8022A974->grid != ((void *)0)) {
        func_000016C4(D_8022A974->grid->data);
        func_000016C4(D_8022A974->grid);
    }
    g = func_00001330(8);
    D_8022A974->grid = g;
    g->h4 = -1;
    func_0029B388(0xA0, 0x78);
}
