typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 button;
    s8 stick_x;
    s8 stick_y;
    u8 errno;
} OSContPad;
extern OSContPad D_800C4BF0[4];
void D_8008A354(void);
void func_0001a77c(void);
void bcopy(const void *src, void *dst, s32 len);
void func_0001aa00(OSContPad *contdata) {
    D_8008A354();
    bcopy(D_800C4BF0, contdata, sizeof(OSContPad) * 4);
    func_0001a77c();
}
