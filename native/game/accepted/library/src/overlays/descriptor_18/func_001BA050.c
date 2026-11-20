typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 used;
    u8 value;
    u16 id;
} ScriptSlot;
extern ScriptSlot D_8022A1F0[];
void bzero(void *p, s32 size);
void func_001BA050(void) {
    bzero(D_8022A1F0, 0x20);
}
void bzero(void *p, s32 n);
