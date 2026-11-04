#ifndef LORDLY_SAVE_FORMAT_H
#define LORDLY_SAVE_FORMAT_H
/* Shared by the desktop engine and the source-built game save runtime. */
#define LC_SAVE_NORMAL_BYTES 0x1850U
#define LC_SAVE_EXPANDED_NORMAL_BYTES 0x1870U
#define LC_SAVE_SUSPEND_BYTES 0x4AE8U
#define LC_SAVE_EXPANDED_SUSPEND_BYTES 0x4B08U
#define LC_SAVE_LARGE_SUSPEND_BYTES 22820U
#define LC_SAVE_CAPACITY_SUSPEND_BYTES 27256U
#define LC_SAVE_CUSTOM_FLAG_BYTES 64U
#define LC_SAVE_CUSTOM_FLAG_OFFSET 0x1870U
#define LC_SAVE_CUSTOM_NORMAL_BYTES 0x18B0U
#define LC_SAVE_CUSTOM_SUSPEND_BYTES (LC_SAVE_EXPANDED_SUSPEND_BYTES + LC_SAVE_CUSTOM_FLAG_BYTES)
#define LC_SAVE_CUSTOM_LARGE_SUSPEND_BYTES (LC_SAVE_LARGE_SUSPEND_BYTES + LC_SAVE_CUSTOM_FLAG_BYTES)
#define LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES (LC_SAVE_CAPACITY_SUSPEND_BYTES + LC_SAVE_CUSTOM_FLAG_BYTES)
#define LC_SAVE_CUSTOM_IMAGE_MARKER 0x7CB8U
#define LC_SAVE_DISCOVERY_BYTES 32U
#define LC_SAVE_LEGACY_DISCOVERY_BIT 43196U
#define LC_SAVE_IMAGE_BYTES 0x8000U
#define LC_SAVE_IMAGE_MARKER 0x7BF8U
#ifdef __cplusplus
extern "C" {
#endif
unsigned int lc_save_record_offset(unsigned int slot, unsigned int expanded);
unsigned int lc_save_record_size(unsigned int slot, unsigned int expanded);
int lc_save_record_seal(unsigned char *record, unsigned int capacity,
                        unsigned int slot, unsigned int expanded);
int lc_save_record_valid(const unsigned char *record, unsigned int capacity,
                         unsigned int slot, unsigned int expanded);
/* Convert one valid legacy record in place, retaining every payload byte.
 * Failure leaves the complete buffer unchanged. No cartridge I/O occurs here.
 * The caller provides the expanded capacity; options retain their old format.
 */
int lc_save_expand_record(unsigned char *record, unsigned int capacity, unsigned int slot);
/* Returns0 retail,1 LC05 discovery,2 LC06 compressed,3 LC07 custom bank,
 *4 LC08 custom compressed; -1 incomplete/unsupported. Invalid individual saves
 * remain invalid. expand_image retains the old0=>1 adapter; upgrade_image in
 * save_compression.h performs the transactional conversion to formats3/4.
 */
int lc_save_image_version(const unsigned char *image, unsigned int capacity);
int lc_save_expand_image(unsigned char *image, unsigned int capacity);
/* Formats0..3 retain their historical byte layout. Formats4/5/6 append a
 * separate64-byte custom bank to basic/capacity/large records respectively.
 * Valid input only; failure leaves the record unchanged. */
int lc_save_upgrade_record(unsigned char *record, unsigned int capacity,
                           unsigned int slot, unsigned int old_format);
/* Pak records use the same payload and signatures but checksum seeds are zero.
 * Decode accepts legacy or expanded data and reseals for the destination slot.
 * Failure does not modify the buffer; the caller supplies expanded capacity.
 */
int lc_save_pak_decode(unsigned char *record, unsigned int capacity, unsigned int slot);
int lc_save_pak_encode(unsigned char *record, unsigned int capacity, unsigned int slot);
#ifdef __cplusplus
}
#endif
#endif
