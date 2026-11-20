typedef unsigned char u8;
typedef unsigned short u16;
extern struct { u8 pad[4]; u16 state; } *D_800C4BBC;
void func_0004e40c(void) {
    if (D_800C4BBC != ((void *)0) && D_800C4BBC->state == 13) {
        D_800C4BBC->state = 2;
    }
}
