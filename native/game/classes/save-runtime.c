/* Expanded adaptations of the accepted boot_resource_checksum_chain producer
 * (gameplay-imports.json). read_save_record follows 0000581C/00005978/00005A88;
 * write_save_record follows its three copy/dirty-flag services; check_slot
 * follows 00005760. Sizes, signatures and discovery migration are extended.
 * The linker binds existing game services and globals;
 * no desktop path, external Decomp tree, or Expansion Pak address is required.
 */
#include "save_format.h"
#include "save_compression.h"
typedef unsigned char SaveByte;
typedef unsigned int SaveWord;
extern SaveByte *volatile lc_save_cache;
extern volatile SaveByte lc_save_dirty;
extern SaveByte lc_discovery_flags[], lc_legacy_discovery[];
void *lc_native_save_alloc(SaveWord size);
void lc_native_sram_io(SaveWord offset, void *data, SaveWord size, SaveWord write);
void lc_native_main_save(SaveByte *data);
void lc_native_save_free(void *data);
void lc_native_save_cleanup(SaveWord commit);
void lc_native_options_init(SaveByte *data);
void lc_native_options_load(SaveByte *data);
void lc_native_options_save(SaveByte *data);
void lc_native_clear(void *data, SaveWord size);
typedef void (*SaveCallback)(SaveByte *data);
typedef struct SaveCallbacks {
    SaveCallback init, load, save;
    SaveWord offset;
    SaveCallback quick_load, quick_save;
    SaveWord quick_offset;
} SaveCallbacks;
extern SaveCallbacks lc_save_callbacks[13];

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
    if (slot == 15 && lc_save_image_version(cache, LC_SAVE_IMAGE_BYTES) == 2)
        return lc_save_read_compressed(cache, LC_SAVE_IMAGE_BYTES, record, size) == size;
    copy_save_bytes(record, cache + lc_save_record_offset(slot, 1), size);
    return lc_save_record_valid(record, size, slot, 1);
}
static int write_save_record(SaveWord slot, const SaveByte *record)
{
    SaveWord size = lc_save_record_size(slot, 1);
    SaveByte *cache, *scratch;
    int result;
    if (!record || !size) return 0;
    cache = save_cache();
    if (!cache) return 0;
    if (slot == 15) {
        scratch = (SaveByte *)lc_native_save_alloc(LC_SAVE_PACK_COMMIT_WORK_BYTES(size));
        if (!scratch) return 0;
        result = lc_save_commit_compressed(cache, LC_SAVE_IMAGE_BYTES, record, size,
                                          scratch, LC_SAVE_PACK_COMMIT_WORK_BYTES(size));
        lc_native_save_free(scratch);
        if (!result) return 0;
        lc_save_dirty = 1;
        return 1;
    }
    copy_save_bytes(cache + lc_save_record_offset(slot, 1), record, size);
    lc_save_dirty = 1;
    return 1;
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
int lc_save_write_suspend(const SaveByte *record) { return write_save_record(15, record); }
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
void lc_discovery_init(SaveByte *record)
{
    SaveWord i;
    for (i = 0; i < LC_SAVE_DISCOVERY_BYTES; ++i) lc_discovery_flags[i] = 0;
    /* Runs after the original initializer, which establishes the game's initial
     * state and serializes it. Retain its first eleven discovery bytes.
     */
    copy_save_bytes(lc_discovery_flags, lc_legacy_discovery, 11);
    copy_save_bytes(record, lc_discovery_flags, LC_SAVE_DISCOVERY_BYTES);
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
void lc_clear_game_state(void *data, SaveWord size)
{
    SaveWord i;
    lc_native_clear(data, size);
    for (i = 0; i < LC_SAVE_DISCOVERY_BYTES; ++i) lc_discovery_flags[i] = 0;
}

static SaveWord save_word(const SaveByte *p)
{
    return ((SaveWord)p[0] << 24) | ((SaveWord)p[1] << 16) | ((SaveWord)p[2] << 8) | p[3];
}
static void save_put_word(SaveByte *p, SaveWord value)
{
    p[0] = (SaveByte)(value >> 24); p[1] = (SaveByte)(value >> 16);
    p[2] = (SaveByte)(value >> 8); p[3] = (SaveByte)value;
}
static void clear_save_bytes(SaveByte *p, SaveWord size)
{
    while (size--) *p++ = 0;
}
static void dispatch_save_callbacks(SaveByte *record, SaveWord operation, SaveWord quick)
{
    SaveWord i;
    SaveCallbacks *row;
    SaveCallback callback;
    for (i = 0; i < 13; ++i) {
        row = lc_save_callbacks + i;
        callback = quick ? (operation == 1 ? row->quick_load : row->quick_save) :
                   (operation == 0 ? row->init : operation == 1 ? row->load : row->save);
        if (callback) callback(record + (quick ? LC_SAVE_EXPANDED_NORMAL_BYTES + row->quick_offset :
                                                12 + row->offset));
    }
}
/* Original boot initialization treats an image as new only when none of its
 * four signatures is recognized. Check signatures separately from checksums so
 * a damaged slot is repaired individually without erasing its valid neighbors.
 */
int lc_save_headers_missing(void)
{
    SaveByte *cache = save_cache(), *record;
    SaveWord i, slot, a, b;
    if (!cache) return 0; /* Damaged layout: all mutation remains blocked. */
    if (lc_save_image_version(cache, LC_SAVE_IMAGE_BYTES) == 2) return 0;
    for (i = 0; i < 4; ++i) {
        slot = i < 2 ? i : i == 2 ? 15 : 14;
        record = cache + lc_save_record_offset(slot, 1);
        a = save_word(record + 4); b = save_word(record + 8);
        if ((a == 0x4C433035U && b == 0x4F473321U) ||
            (a == 0x51756573U && b == 0x744F4733U)) return 0;
    }
    return 1;
}
void lc_save_initialize_slot(SaveWord slot)
{
    SaveWord size = lc_save_record_size(slot, 1);
    SaveByte *record, *cache;
    if (!size || !(cache = save_cache())) return;
    if (slot == 15) {
        if (lc_save_image_version(cache, LC_SAVE_IMAGE_BYTES) == 2) {
            record = (SaveByte *)lc_native_save_alloc(size);
            if (!record) return;
            /* Preserve a damaged/unsupported compressed save for recovery. */
            if (read_save_record(15, record)) {
                save_put_word(record, 0);
                write_save_record(15, record);
            }
            lc_native_save_free(record);
            return;
        }
        /* Clearing suspend changes only its twelve-byte header, as before. */
        record = cache + lc_save_record_offset(slot, 1);
        save_put_word(record, 0);
        save_put_word(record + 4, 0x4C433035U); save_put_word(record + 8, 0x4F473321U);
        lc_save_dirty = 1;
        return;
    }
    record = (SaveByte *)lc_native_save_alloc(size);
    if (!record) return;
    clear_save_bytes(record, size);
    if (slot == 14) lc_native_options_init(record + 12);
    else dispatch_save_callbacks(record, 0, 0);
    lc_save_sign(slot, record);
    write_save_record(slot, record);
    lc_native_save_free(record);
}
int lc_save_check_slot(SaveWord slot)
{
    SaveWord size = lc_save_record_size(slot, 1);
    SaveByte *cache = save_cache(), *record;
    int valid;
    if (!cache || !size) return 0;
    if (slot == 15 && lc_save_image_version(cache, LC_SAVE_IMAGE_BYTES) == 2) {
        record = (SaveByte *)lc_native_save_alloc(size);
        if (!record) return 0;
        valid = read_save_record(slot, record);
        lc_native_save_free(record);
        return valid;
    }
    valid = lc_save_record_valid(cache + lc_save_record_offset(slot, 1), size, slot, 1);
    if (!valid) lc_save_initialize_slot(slot);
    return valid;
}
static int save_game_slot(SaveWord slot)
{
    SaveWord size = lc_save_record_size(slot, 1), counter = 0;
    SaveByte *record, *cache;
    int result;
    if (!size || !(cache = save_cache())) return 0;
    if (slot < 2) {
        /* The counter comes from the prior record, never recycled heap bytes. */
        if (lc_save_record_valid(cache + lc_save_record_offset(slot, 1), size, slot, 1))
            counter = save_word(cache + lc_save_record_offset(slot, 1) + 12);
        ++counter;
        if (!counter) counter = ~0U;
    }
    record = (SaveByte *)lc_native_save_alloc(size);
    if (!record) return 0;
    clear_save_bytes(record, size);
    if (slot == 14) lc_native_options_save(record + 12);
    else {
        save_put_word(record + 12, counter);
        dispatch_save_callbacks(record, 2, 0);
        if (slot == 15) dispatch_save_callbacks(record, 2, 1);
    }
    lc_save_sign(slot, record);
    result = write_save_record(slot, record);
    lc_native_save_free(record);
    return result;
}
static void load_game_slot(SaveWord slot)
{
    SaveWord size = lc_save_record_size(slot, 1);
    SaveByte *record;
    if (!size) return;
    record = (SaveByte *)lc_native_save_alloc(size);
    if (!record) return;
    if (read_save_record(slot, record)) {
        if (slot == 14) lc_native_options_load(record + 12);
        else if (slot != 15 || save_word(record)) {
            dispatch_save_callbacks(record, 1, 0);
            if (slot == 15) dispatch_save_callbacks(record, 1, 1);
        }
    }
    lc_native_save_free(record);
}
int lc_save_suspend(void) { return save_game_slot(15); }
void lc_load_suspend(void) { load_game_slot(15); }
int lc_save_game(SaveWord slot)
{
    int result = save_game_slot(slot);
    lc_native_save_cleanup(0x37081383U);
    return result;
}
/* The field-menu continuation uses -3 to exit and -2 to resume gameplay.
 * Its patched call preserves this result instead of unconditionally exiting.
 */
int lc_save_suspend_menu(void) { return lc_save_game(15) ? 0xFFFDU : 0xFFFEU; }
void lc_load_game(SaveWord slot)
{
    load_game_slot(slot);
    lc_native_save_cleanup(0x37081383U);
}
SaveByte *lc_save_summary(SaveWord slot)
{
    SaveByte *cache, *record, *summary = 0, *temporary = 0;
    SaveWord size = lc_save_record_size(slot, 1);
    if ((slot < 2 || slot == 15) && (cache = save_cache())) {
        record = cache + lc_save_record_offset(slot, 1);
        if (slot == 15 && lc_save_image_version(cache, LC_SAVE_IMAGE_BYTES) == 2) {
            temporary = (SaveByte *)lc_native_save_alloc(size);
            record = temporary;
            if (record && !read_save_record(slot, record)) { lc_native_save_free(record); record = 0; temporary = 0; }
        }
        if (record && lc_save_record_valid(record, size, slot, 1) && save_word(record + (slot == 15 ? 0 : 12))) {
            summary = (SaveByte *)lc_native_save_alloc(26);
            if (summary) copy_save_bytes(summary, record + 12 + lc_save_callbacks[0].offset, 26);
        }
        if (temporary) lc_native_save_free(temporary);
    }
    lc_native_save_cleanup(0x37081383U);
    return summary;
}
int lc_pak_export(SaveWord slot, SaveByte *file, SaveWord capacity)
{
    SaveByte *cache, *record;
    if (slot >= 2 || !file || capacity < 0x20U + LC_SAVE_EXPANDED_NORMAL_BYTES) return 0;
    cache = save_cache();
    if (!cache) { lc_native_save_cleanup(0x37081383U); return 0; }
    record = cache + lc_save_record_offset(slot, 1);
    if (!lc_save_record_valid(record, LC_SAVE_EXPANDED_NORMAL_BYTES, slot, 1)) {
        lc_native_save_cleanup(0x37081383U); return 0;
    }
    clear_save_bytes(file, capacity);
    copy_save_bytes(file + 0x20, record, LC_SAVE_EXPANDED_NORMAL_BYTES);
    lc_save_pak_encode(file + 0x20, capacity - 0x20, slot);
    copy_save_bytes(file, record + 12 + lc_save_callbacks[0].offset, 26);
    lc_native_save_cleanup(0x37081383U);
    return 1;
}
int lc_pak_import(SaveWord slot, const SaveByte *file, SaveWord size)
{
    SaveByte *record;
    SaveWord bytes;
    int valid;
    if (slot >= 2 || !file || size < 0x20U + LC_SAVE_NORMAL_BYTES) return 0;
    bytes = size - 0x20;
    if (bytes > LC_SAVE_EXPANDED_NORMAL_BYTES) bytes = LC_SAVE_EXPANDED_NORMAL_BYTES;
    /* A short expanded record must not be completed with fabricated zeroes. */
    if (save_word(file + 0x24) == 0x4C433035U && bytes < LC_SAVE_EXPANDED_NORMAL_BYTES) return 0;
    record = (SaveByte *)lc_native_save_alloc(LC_SAVE_EXPANDED_NORMAL_BYTES);
    if (!record) return 0;
    clear_save_bytes(record, LC_SAVE_EXPANDED_NORMAL_BYTES);
    copy_save_bytes(record, file + 0x20, bytes);
    valid = lc_save_pak_decode(record, LC_SAVE_EXPANDED_NORMAL_BYTES, slot);
    if (valid) {
        if (save_cache()) write_save_record(slot, record);
        else valid = 0;
    }
    lc_native_save_free(record);
    lc_native_save_cleanup(0x37081383U);
    return valid;
}
