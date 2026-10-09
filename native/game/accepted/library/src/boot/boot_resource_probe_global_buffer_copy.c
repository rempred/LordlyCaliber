#include "common/types.h"

extern void *resource_alloc(u32 bytes);
extern void func_0001A4F0(s32 offset, void *destination, u32 bytes, s32 mode);
extern void func_00023460(const void *source, void *destination, u32 bytes);
extern u8 *D_800A83B8;

void boot_resource_probe_global_buffer_copy(void *destination, u32 sourceOffset, u32 bytes)
{
    s32 offset;

    if (D_800A83B8 == 0) {
        D_800A83B8 = resource_alloc(0x8000);
        offset = 0;
        do {
            func_0001A4F0(offset, D_800A83B8 + offset, 0x100, 0);
            offset += 0x100;
        } while (offset < 0x8000);
    }
    func_00023460(D_800A83B8 + sourceOffset, destination, bytes);
}
