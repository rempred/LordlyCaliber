typedef unsigned long long u64;
typedef struct {
    u64 pad[0x1B0 / 8];
} OSThread;
extern OSThread D_800BBE80;
void osStartThread(OSThread *t);
void func_0001a17c(void) {
    osStartThread(&D_800BBE80);
}
