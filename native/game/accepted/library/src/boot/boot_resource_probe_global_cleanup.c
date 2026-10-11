#include "common/types.h"

extern void boot_resource_probe_chunk_callback_walk(u8 *buffer);
extern void resource_free(void *record);
extern u8 *D_800A83B8;
extern u8 D_800A83BC;

void boot_resource_probe_global_cleanup(u32 key)
{
    if (D_800A83BC == 1) {
        if (key == 0x37081383) {
            boot_resource_probe_chunk_callback_walk(D_800A83B8);
        }
        D_800A83BC = 0;
    }
    if (D_800A83B8 != 0) {
        resource_free(D_800A83B8);
        D_800A83B8 = 0;
    }
}
