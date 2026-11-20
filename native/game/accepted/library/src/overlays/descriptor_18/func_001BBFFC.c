typedef unsigned char u8;
typedef signed int s32;
typedef struct { u8 b[0x76]; } ScriptObj;
extern ScriptObj D_8022A2E8[];
ScriptObj *func_001BBFFC(s32 i) {
    return &D_8022A2E8[i];
}
