#ifndef LORDLY_CAPACITY_SAVE_H
#define LORDLY_CAPACITY_SAVE_H

#define LC_CAPACITY_QUICK_BYTES 9300U
#define LC_CAPACITY_TAG_BYTES 664U
#define LC_CAPACITY_MISSION_BYTES 11036U
#if LC_CUSTOM_STORY_FLAGS
#define LC_CAPACITY_NORMAL_BYTES 0x18B0U
#define LC_CAPACITY_TARGET_FORMAT 5U
#define LC_CAPACITY_SOURCE_FORMAT 4U
#else
#define LC_CAPACITY_NORMAL_BYTES 6256U
#define LC_CAPACITY_TARGET_FORMAT 3U
#define LC_CAPACITY_SOURCE_FORMAT 1U
#endif
#define LC_CAPACITY_TAG_OFFSET (LC_CAPACITY_NORMAL_BYTES + LC_CAPACITY_QUICK_BYTES)
#define LC_CAPACITY_MISSION_OFFSET (LC_CAPACITY_TAG_OFFSET + LC_CAPACITY_TAG_BYTES)

/* Upgrade the discovery-expanded retail suspend layout (format1) in place.
 * Source record must be valid; unsupported formats and short buffers are left
 * untouched. All prior packed fields, snapshot bytes and padding are retained.
 */
int lc_save_expand_capacity(unsigned char *record, unsigned int capacity);
void lc_capacity_tags_save(unsigned char *data);
void lc_capacity_tags_load(const unsigned char *data);

#endif
