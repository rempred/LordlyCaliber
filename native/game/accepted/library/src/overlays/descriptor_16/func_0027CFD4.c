typedef signed short s16;
typedef unsigned short u16;
typedef struct {
    s16 *list;
    u16 key;
} Find27;
s16 func_0027CFD4(Find27 *f) {
    s16 i;
    if (f == ((void *)0) || f->list == ((void *)0)) {
        return 0;
    }
    for (i = 0; f->list[i] != 0; i++) {
        if (f->list[i] == f->key) {
            return f->key;
        }
    }
    return 0;
}
