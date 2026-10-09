typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

typedef struct { u8 bytes[52]; } Record52;
typedef struct { u8 bytes[56]; } Record56;
typedef struct { u32 key[3]; } ResourceKeys;

extern Record52 g_func_0019554C_records_52[];
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
    u32 keys[3];

    if (mode == 0) {
        record = g_func_0019554C_records_56[record_index].bytes;
    } else {
        record = g_func_0019554C_records_52[record_index].bytes;
    }

    code = record[0x11];
    if (((u8)code < 0x51) || ((u32)(code - 0x72) < 3)) {
      attempts = 0x20;
      /* Keep the byte mode as a word outside the retry loop. KMC otherwise
       * leaves the mode-one constant inside the conditional second scan. */
      selector = mode;
      do {
        u8 *decoded;
        u8 *chosen;
        s32 skip;
        s32 duplicate;
        s32 candidate;
        u32 *key;

        key = &keys[record[0x14]];
        *(ResourceKeys *)keys = D_8018FEAC;
        decoded = func_0002E138(*key);
        skip = rand() % 256;
        chosen = decoded;
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
        for (candidate = 1; candidate < 100; candidate++) {
            if ((selector == 0) && (record_index != candidate) &&
                (g_func_0019554C_records_56[candidate].bytes[0x11] != 0) &&
                (func_0002C9E0((const char *)record,
                                 (const char *)g_func_0019554C_records_56[candidate].bytes) == 0)) {
                duplicate++;
                break;
            }
        }
        if (duplicate == 0) {
            for (candidate = 1; candidate < 100; candidate++) {
                if ((selector == 1) && (record_index != candidate) &&
                    (g_func_0019554C_records_52[candidate].bytes[0x11] != 0) &&
                    (func_0002C9E0((const char *)record,
                                     (const char *)g_func_0019554C_records_52[candidate].bytes) == 0)) {
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
