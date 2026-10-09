typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

extern void *D_800AF378;
extern void *D_800AF37C;
extern u32 D_800AF380;
extern u32 D_800AF384;
extern u32 D_800AF388;
extern u32 D_800AF38C;
extern u16 crc;
extern long func_0000C990(void *, void *, long, int);
struct interfacing;
extern void func_0000C310(struct interfacing *);

u32 boot_resource_op_dispatch(void *arg0, void *arg1, u32 arg2, u32 arg3,
                              void *arg4, u32 operation)
{
    u32 mode_value;
    D_800AF388 = 13;
    D_800AF378 = arg0;
    D_800AF37C = arg1;
    D_800AF380 = arg2;
    D_800AF384 = arg3;
    D_800AF38C = operation;
    switch (operation) {
    case 0:
    case 8:
        func_0000C990(arg0, arg1, (long)arg2, 2);
        break;
    case 6:
        /* The incoming pointer is dead on these decode paths; its next phase
         * addresses the actual mode word, preserving KMC's argument register. */
        arg0 = &D_800AF388;
        mode_value = 11;
        goto set_mode;
    case 1:
    case 4:
    case 7:
        arg0 = &D_800AF388;
        mode_value = 12;
set_mode:
        *(u32 *)arg0 = mode_value;
    case 2:
    case 3:
    case 5:
    default:
        func_0000C310((struct interfacing *)&D_800AF378);
        break;
    }
    return crc;
}

static void func_0000BF48(u8 *path, int length)
{
    if (length > 0) {
        int colon = ':';
        int slash = '/';
        length += (int)path;
        do {
            int character = *path;
            if (character == colon) {
                *path = slash;
            } else if (character == slash) {
                *path = colon;
            }
            path++;
        } while ((int)path < length);
    }
}
