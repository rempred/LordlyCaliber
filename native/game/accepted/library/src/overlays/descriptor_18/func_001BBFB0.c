typedef unsigned char u8;
typedef signed int s32;
typedef struct { u8 b[0x76]; } ScriptObj;
extern ScriptObj D_8022A2E8[];
void bzero(void *p, s32 size);
ScriptObj *func_001BBFB0(s32 i) {
    ScriptObj *o = &D_8022A2E8[i];
    bzero(o, sizeof(ScriptObj));
    return o;
}
