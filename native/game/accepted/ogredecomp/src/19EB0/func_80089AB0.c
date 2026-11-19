typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile s32 vs32;
extern void **D_800E8290;
extern u32 D_800A9E80;
extern u32 D_800C4BC4;
extern void *D_800E8210;
void func_80089A30(u32 retrace_num);
void func_80089528(u8 frameBufferNum);
void *osViGetCurrentFramebuffer(void);
void func_80089AB0(void **framebuf, u32 framebufnum) {
    u32 cnt;
    D_800E8290 = framebuf;
    D_800A9E80 = framebufnum;
    D_800C4BC4 = 0;
    D_800E8210 = framebuf[0];
    func_80089A30(1);
    func_80089528(framebufnum);
    if (framebufnum > 1) {
        for (cnt = 0; cnt < framebufnum - 1; cnt++) {
            if (D_800E8290[cnt] != D_800E8290[cnt + 1]) {
                break;
            }
        }
        if (cnt == framebufnum - 1) {
            func_80089528(1);
        } else {
            for (cnt = 0; cnt < framebufnum; cnt++) {
                if (osViGetCurrentFramebuffer() != D_800E8290[cnt]) {
                    D_800C4BC4 = cnt;
                    D_800E8210 = D_800E8290[cnt];
                    break;
                }
            }
        }
    }
}
