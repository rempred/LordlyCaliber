/* Class data is loaded once, before the first main-heap initialization.
 * Later heap resets must retain runtime changes to the class records.
 * The exporter supplies the exact data extent and links the original services.
 */
#include "class_runtime_config.h"
typedef unsigned int u32;
void lc_original_heap_init(u32, u32);
void lc_original_pool_add(u32, u32);
void lc_original_secondary_reset(void);
void lc_dma_read(u32, void *, u32);

static void add_reclaimed_storage(void)
{
#if LC_RECLAIM_SIZE != 0
    lc_original_pool_add(LC_RECLAIM_START, LC_RECLAIM_SIZE);
#endif
    lc_original_pool_add(0x8018ECE0U, 0x3D0);
    lc_original_pool_add(0x8018FEC0U, 0x4B0);
}

void lc_heap_reset(u32 start, u32 size)
{
    if (start == 0x80243DB0U && size == 0x1BC250U) {
        start += LC_RESERVED_BYTES;
        size -= LC_RESERVED_BYTES;
    }
    lc_original_heap_init(start, size);
    add_reclaimed_storage();
}

void lc_secondary_reset(void)
{
    lc_original_secondary_reset();
    add_reclaimed_storage();
}

void lc_boot_classes(u32 start, u32 size)
{
    lc_dma_read(LC_ROM_DATA, (void *)0x80243DB0U, LC_RESERVED_BYTES);
    lc_heap_reset(start, size);
}
