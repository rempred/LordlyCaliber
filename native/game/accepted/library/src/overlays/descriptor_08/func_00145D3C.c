typedef signed int s32;
extern s32 *D_801FDA14;
extern s32 D_801FDA18;
typedef struct MapObj {
    void (*fn)(struct MapObj *self);
} MapObj;
void func_00145D3C(void) {
    s32 i;
    MapObj *o;
    for (i = D_801FDA18 - 1; i >= 0; i--) {
        o = ((MapObj **)D_801FDA14)[i];
        if (o != ((void *)0)) {
            o->fn(o);
        }
    }
}
