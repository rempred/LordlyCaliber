#ifndef LORDLY_SAVE_FORMAT_H
#define LORDLY_SAVE_FORMAT_H
/* Shared by the desktop engine and the source-built game save runtime. */
#define LC_SAVE_NORMAL_BYTES 0x1850U
#define LC_SAVE_EXPANDED_NORMAL_BYTES 0x1870U
#define LC_SAVE_SUSPEND_BYTES 0x4AE8U
#define LC_SAVE_EXPANDED_SUSPEND_BYTES 0x4B08U
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
/* Returns 0 for legacy, 1 for the expanded image, -1 for an incomplete or
 * unsupported expanded image. Invalid individual saves remain invalid.
 */
int lc_save_image_version(const unsigned char *image, unsigned int capacity);
int lc_save_expand_image(unsigned char *image, unsigned int capacity);
#ifdef __cplusplus
}
#endif
#endif
