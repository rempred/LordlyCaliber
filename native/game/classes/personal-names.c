/* Adapted from accepted func_00041c20.c. A null identity-name pointer uses the
 * original random-name resource, retry and duplicate-name rules. */
#include "personal_names_config.h"
typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

typedef struct { u8 bytes[52]; } Record52;
typedef struct { u8 bytes[56]; } Record56;
typedef struct { u32 key[3]; } ResourceKeys;

extern Record52 g_func_0019554C_records_52[];
#ifdef LC_CAPACITY
extern u8 lc_enemy_types[];
#endif
extern Record56 g_func_0019554C_records_56[];
extern ResourceKeys D_8018FEAC;
extern char *D_8018EA00[];

extern u8 *func_0002E138(u32 key);
extern s32 rand(void);
extern void func_80093380(void *destination, s32 size);
extern char *func_0002C950(char *destination, const char *source);
extern s32 func_0002C9E0(const char *first, const char *second);
extern void resource_free(void *resource);

void func_00041c20(u8 mode, u8 record_index)
{
    u8 *record;
    s32 code;
    s32 attempts;
    u32 selector;
    u8 *pool;
    s32 stride, count;

    if (mode == 0) {
        pool = (u8 *)g_func_0019554C_records_56;
        stride = 56;
        count = 100;
    } else {
        pool = (u8 *)g_func_0019554C_records_52;
        stride = 52;
        count = LC_ENEMY_COUNT;
    }
    record = pool + record_index * stride;

    code = record[0x11];
    if (D_8018EA00[(u8)code] == 0) {
      attempts = 0x20;
      /* Modes zero and one check their own roster for duplicate names.
       * Other modes use enemy records without a duplicate-name scan. */
      selector = mode;
      do {
        u8 *decoded;
        u8 *chosen;
        s32 skip;
        s32 duplicate;
        s32 candidate;
        s32 name_count;
        u8 *other;

        decoded = func_0002E138(D_8018FEAC.key[record[0x14]]);
        name_count = 256;
        chosen = decoded;
        /* Owned expanded lists have a zero first byte, a version marker and
         * a word count. Retail strings begin immediately and keep 256 names.
         * Keep decoded unchanged: resource_free owns the original pointer. */
        if (decoded[0] == 0) {
            name_count = ((u32 *)decoded)[1];
            chosen += 8;
        }
        skip = rand() % name_count;
        while (skip != 0) {
            while (*chosen++ != 0) {
            }
            skip--;
        }

        func_80093380(record, 0x11);
        func_0002C950((char *)record, (const char *)chosen);
        resource_free(decoded);
        attempts--;

        duplicate = 0;
        if (selector < 2) {
            other = pool + stride;
            for (candidate = 1; candidate < count; candidate++, other += stride) {
                if ((record_index != candidate) &&
#ifdef LC_CAPACITY
                    (selector == 0 || lc_enemy_types[candidate] == 1) &&
#endif
                    (other[0x11] != 0) &&
                    (func_0002C9E0((const char *)record, (const char *)other) == 0)) {
                    duplicate++;
                    break;
                }
            }
        }
        attempts &= -(duplicate != 0);
      } while (attempts != 0);
    } else {
        func_0002C950((char *)record, D_8018EA00[(u8)code]);
    }
}
