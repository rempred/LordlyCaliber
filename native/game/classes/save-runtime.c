/* Expanded save services. The linker binds existing game services and globals;
 * no desktop path, external Decomp tree, or Expansion Pak address is required.
 */
#include "save_format.h"
typedef unsigned char SaveByte;
typedef unsigned int SaveWord;
extern SaveByte *volatile lc_save_cache;
extern volatile SaveByte lc_save_dirty;
extern SaveByte lc_discovery_flags[], lc_legacy_discovery[];
void *lc_native_save_alloc(SaveWord size);
void lc_native_sram_io(SaveWord offset, void *data, SaveWord size, SaveWord write);
void lc_native_main_save(SaveByte *data);

static void copy_save_bytes(SaveByte *to, const SaveByte *from, SaveWord size)
{
    while (size--) *to++ = *from++;
}
static SaveByte *save_cache(void)
{
    SaveWord offset;
    SaveByte *cache = lc_save_cache;
    if (!cache) {
        cache = (SaveByte *)lc_native_save_alloc(LC_SAVE_IMAGE_BYTES);
        if (!cache) return 0;
        lc_save_cache = cache;
        for (offset = 0; offset < LC_SAVE_IMAGE_BYTES; offset += 256)
            lc_native_sram_io(offset, cache + offset, 256, 0);
    }
    /* Reads normalize only the cached copy. The unchanged native flush routine
     * commits the whole image only after a game write sets its dirty flag.
     */
    if (!lc_save_expand_image(cache, LC_SAVE_IMAGE_BYTES)) return 0;
    return cache;
}
static int read_save_record(SaveWord slot, SaveByte *record)
{
    SaveWord size = lc_save_record_size(slot, 1);
    SaveByte *cache;
    if (!record || !size) return 0;
    cache = save_cache();
    if (!cache) return 0;
    copy_save_bytes(record, cache + lc_save_record_offset(slot, 1), size);
    return lc_save_record_valid(record, size, slot, 1);
}
static void write_save_record(SaveWord slot, const SaveByte *record)
{
    SaveWord size = lc_save_record_size(slot, 1);
    SaveByte *cache;
    if (!record || !size) return;
    cache = save_cache();
    if (!cache) return;
    copy_save_bytes(cache + lc_save_record_offset(slot, 1), record, size);
    lc_save_dirty = 1;
}
int lc_save_read_normal(SaveWord slot, SaveByte *record)
{
    return slot < 2 ? read_save_record(slot, record) : 0;
}
int lc_save_read_suspend(SaveByte *record) { return read_save_record(15, record); }
int lc_save_read_options(SaveByte *record) { return read_save_record(14, record); }
void lc_save_write_normal(SaveWord slot, const SaveByte *record)
{
    if (slot < 2) write_save_record(slot, record);
}
void lc_save_write_suspend(const SaveByte *record) { write_save_record(15, record); }
void lc_save_write_options(const SaveByte *record) { write_save_record(14, record); }
void lc_save_copy(SaveByte *record, SaveWord offset, SaveWord size)
{
    SaveByte *cache;
    if (!record || offset > LC_SAVE_IMAGE_BYTES || size > LC_SAVE_IMAGE_BYTES - offset) return;
    cache = save_cache();
    if (cache) copy_save_bytes(record, cache + offset, size);
}
void lc_save_sign(SaveWord slot, SaveByte *record)
{
    lc_save_record_seal(record, lc_save_record_size(slot, 1), slot, 1);
}
void lc_discovery_init(void)
{
    SaveWord i;
    for (i = 0; i < LC_SAVE_DISCOVERY_BYTES; ++i) lc_discovery_flags[i] = 0;
    for (i = 0; i < 11; ++i) lc_legacy_discovery[i] = 0;
}
void lc_discovery_load(const SaveByte *record)
{
    copy_save_bytes(lc_discovery_flags, record, LC_SAVE_DISCOVERY_BYTES);
    copy_save_bytes(lc_legacy_discovery, record, 11);
}
void lc_discovery_save(SaveByte *record)
{
    copy_save_bytes(record, lc_discovery_flags, LC_SAVE_DISCOVERY_BYTES);
}
void lc_save_main_payload(SaveByte *record)
{
    copy_save_bytes(lc_legacy_discovery, lc_discovery_flags, 11);
    lc_native_main_save(record);
}
