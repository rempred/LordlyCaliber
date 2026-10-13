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
/* Linker-provided absolute end avoids GCC materializing both the positive
 * address and its negation. Keep this tiny resident loader within its owner. */
extern unsigned char lc_class_data_end[];

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
    /* Every retail main-heap reset ends at 0x80400000. The title/new-game
     * transition starts at 0x801BA710, below the persistent class allocation.
     * Keep the upper arena primary; the lower span remains a secondary pool
     * until the original overlay reset discards/rebuilds those secondary pools.
     */
    lc_original_heap_init((u32)lc_class_data_end,
                          start + size - (u32)lc_class_data_end);
    add_reclaimed_storage();
    if (start < 0x80243DB0U)
        lc_original_pool_add(start, 0x80243DB0U - start);
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
