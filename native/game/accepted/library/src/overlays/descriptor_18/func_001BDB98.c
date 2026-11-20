typedef unsigned char u8;
typedef signed int s32;
typedef struct { u8 b[0x76]; } ScriptObj;
extern ScriptObj D_8022A2E8[];
void bzero(void *p, s32 size);
void func_001BDB98(void) {
    bzero(D_8022A2E8, 4 * sizeof(ScriptObj));
}
