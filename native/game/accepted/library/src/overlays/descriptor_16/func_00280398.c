typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct Task27 {
    u8 unk00[4];
    u16 state;
    u16 next;
    void (*fn)(s32, struct Task27 *);
    s16 hC;
    s16 hE;
    s16 h10;
    u8 unk12[0x26];
} Task27;
void func_0027FD7C(s32 a, Task27 *t);
Task27 *func_0027CEDC(void (*fn)(s32, Task27 *));
void func_00280398(void) {
    func_0027CEDC(func_0027FD7C);
}
