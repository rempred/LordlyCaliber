typedef unsigned char u8;
typedef unsigned int u32;
extern void **D_800E8290;
extern u32 D_800A9E80;
extern u32 D_800C4BC4;
extern void *D_800E8210;
void func_00019e30(u32 retrace_num);
void set_byte_800f918d(u8 frameBufferNum);
void *osViGetCurrentFramebuffer(void);
void func_00019eb0(void **framebuf, u32 framebufnum) {
    u32 cnt;
    D_800E8290 = framebuf;
    D_800A9E80 = framebufnum;
    D_800C4BC4 = 0;
    D_800E8210 = framebuf[0];
    func_00019e30(1);
    set_byte_800f918d(framebufnum);
    if (framebufnum > 1) {
        for (cnt = 0; cnt < framebufnum - 1; cnt++) {
            if (D_800E8290[cnt] != D_800E8290[cnt + 1]) {
                break;
            }
        }
        if (cnt == framebufnum - 1) {
            set_byte_800f918d(1);
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
