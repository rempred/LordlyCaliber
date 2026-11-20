typedef unsigned char u8;
typedef signed int s32;
typedef struct { u8 b[0x76]; } ScriptObj;
extern ScriptObj D_8022A2E8[];
typedef void (*ScriptObjFn)(ScriptObj *o);
extern ScriptObjFn D_8022A020[];
void func_001BDC6C(void) {
    s32 i;
    ScriptObjFn fn;
    for (i = 0; i < 4; i++) {
        if (D_8022A2E8[i].b[0] != 0) {
            fn = D_8022A020[D_8022A2E8[i].b[0]];
            if (fn != ((void *)0)) {
                fn(&D_8022A2E8[i]);
            }
        }
    }
}
