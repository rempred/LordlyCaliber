#ifndef LORDLY_SAVE_COMPRESSION_H
#define LORDLY_SAVE_COMPRESSION_H
/* Bounded, lossless save-record codec. Callers supply private scratch/output;
 * failure may change scratch output, never the source. Do not publish a save
 * until encoding and readback both succeed. No allocator or device I/O here.
 * Streams use the game's existing LZSS token grammar with strict bounds.
 */
#define LC_SAVE_PACK_MAX_BYTES 32768U
#define LC_SAVE_PACK_HEADS 4096U
#define LC_SAVE_PACK_HEADER_BYTES 32U
#define LC_SAVE_PACK_WORK_BYTES(size) (2U * (LC_SAVE_PACK_HEADS + (size)))
#define LC_SAVE_PACK_IMAGE_BYTES 32768U
#define LC_SAVE_PACK_SUSPEND_OFFSET 0x30F0U
#define LC_SAVE_PACK_IMAGE_MARKER 0x7FF0U
#define LC_SAVE_PACK_SUSPEND_CAPACITY (LC_SAVE_PACK_IMAGE_MARKER - LC_SAVE_PACK_SUSPEND_OFFSET)
#define LC_SAVE_PACK_COMMIT_WORK_BYTES(size) (LC_SAVE_PACK_SUSPEND_CAPACITY + (((size) + 1U) & ~1U) + LC_SAVE_PACK_WORK_BYTES(size))
#ifdef __cplusplus
extern "C" {
#endif
unsigned int lc_save_lzss_encode(const unsigned char *source, unsigned int size,
                               unsigned char *output, unsigned int capacity,
                               unsigned short *work);
int lc_save_lzss_decode(unsigned char *output, unsigned int size,
                        const unsigned char *source, unsigned int length);
unsigned int lc_save_frame_encode(const unsigned char *source, unsigned int size,
                                 unsigned char *output, unsigned int capacity,
                                 unsigned short *work);
/* Checks complete frame header/stored CRC and bounds; returns logical size.
 * Loading must additionally decode and check the logical CRC before dispatch.
 */
unsigned int lc_save_frame_size(const unsigned char *source, unsigned int capacity);
unsigned int lc_save_frame_decode(unsigned char *output, unsigned int capacity,
                                 const unsigned char *source, unsigned int length);
unsigned int lc_save_data_crc(const unsigned char *source, unsigned int length);
/* Transactional physical-image replacement. Every failure leaves all32768
 * image bytes and the source unchanged. Existing image must be expanded first.
 * Scratch must be separate from source/image and two-byte aligned.
 */
int lc_save_commit_compressed(unsigned char *image, unsigned int capacity,
                             const unsigned char *source, unsigned int size,
                             unsigned char *scratch, unsigned int scratch_size);
/* Decode/verify the compressed logical record into private output, or return0.
 * A caller must not dispatch any game load callback after a failure.
 */
unsigned int lc_save_read_compressed(const unsigned char *image, unsigned int capacity,
                                    unsigned char *output, unsigned int output_capacity);
#ifdef __cplusplus
}
#endif
#endif
