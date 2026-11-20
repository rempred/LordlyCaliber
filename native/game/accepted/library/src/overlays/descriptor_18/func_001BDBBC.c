typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct { u8 b[0x76]; } ScriptObj;
extern ScriptObj D_8022A2E8[];
void func_001BDBBC();
typedef void (*ScriptObjFn)(ScriptObj *o);
typedef struct {
    u32 w0;
    u32 w1;
} GfxBB;
extern GfxBB *gTitleCardDisplayListCursor;
extern u8 D_80229FB8[];
extern ScriptObjFn D_80229FA4[];
void func_001BDBBC(void) {
    GfxBB *g;
    s32 i;
    ScriptObjFn fn;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDE000000;
    g->w1 = (u32)D_80229FB8;
    for (i = 0; i < 4; i++) {
        fn = D_80229FA4[D_8022A2E8[i].b[0]];
        if (fn != ((void *)0)) {
            fn(&D_8022A2E8[i]);
        }
    }
}
