typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

/* Partial views derived from this owner's actual memory accesses. */
typedef struct InputView {
    u8 unknown00[8];
    u8 *buffer;
    int limit;
    int position;
} InputView;

typedef struct ResourceRecord {
    u8 byte00;
    u8 bytes01[5];
    u8 unknown06[2];
    u32 word08;
    u32 word0C;
    u32 word10;
    u8 byte14;
    u8 byte15;
    u8 bytes16[0x100];
    u16 half116;
    u32 word118;
    u8 byte11C;
    u8 byte11D;
    u8 unknown11E[2];
    u32 word120;
    u16 half124;
    u16 half126;
    u16 half128;
    u8 unknown12A[2];
} ResourceRecord;

extern u8 *D_800AF390;
extern const u8 D_800AE0E8[];
extern const u8 D_800AE108[];
extern const u8 g_boot_resource_lzss_error_anchor[];
extern void func_80093380(void *, u32);
extern u32 func_0000F970(void *, u32, u32, InputView *);
extern void func_00023460(const void *, void *, u32);
extern void func_0000BFC0(const u8 *);
extern u8 *func_0000C938(u8 *, u8);
extern u8 *strcat(u8 *, u8 *);
extern u8 *func_0000F450(u8 *, const u8 *);
extern void func_0000BF48(u8 *, int);

static u16 func_0000BB10(void);
static u32 func_0000BB44(void);
static void func_0000BBA8(u8 *, int);
static void func_0000BC3C(u8 *, int);

#define INPUT_BYTE(input) ((input)->buffer[(input)->position++])

static inline u32 input_byte(InputView *input)
{
    return INPUT_BYTE(input);
}

int boot_resource_tag_record_decode(InputView *input, ResourceRecord *record)
{
    u8 raw[0x1000];
    u8 prefix[0x400];
    int prefix_length = 0;
    int tag;
    int name_length;
    int extension_length;
    int count;
    u8 *extension_start;
    u8 *prefix_base;
    const u8 *failure_message;
    int extension_marker;

    func_80093380(record, 0x12C);
    tag = input->position < input->limit ? INPUT_BYTE(input) : -1;
    if (tag == -1) {
        goto invalid_record;
    }
    if (tag == 0) {
        goto invalid_record;
    }
    if (tag & 0x80) {
        u32 first_value;
        u32 second_value;
        record->byte00 = tag;
        /* Each call sequences its byte read; the pinned KMC compiler evaluates
         * this expression in the observed low-to-high byte order. */
        first_value = input_byte(input) | (input_byte(input) << 8) |
                      (input_byte(input) << 16) | (input_byte(input) << 24);
        record->word08 = first_value;
        {
            u32 byte0 = input_byte(input);
            u32 byte1 = input_byte(input);
            u32 byte2 = input_byte(input);
            u32 byte3 = input_byte(input);
            byte1 <<= 8;
            byte2 <<= 16;
            byte3 <<= 24;
            second_value = byte0 | byte1 | byte2 | byte3;
        }
        record->word0C = second_value;
        return 1;
    }

    if (func_0000F970(raw + 1, 1, tag - 1, input) < (u32)(tag - 1)) {
        failure_message = D_800AE0E8;
        goto report_read_error;
    }
    D_800AF390 = raw + 0x15;
    record->byte15 = raw[0x14];
    if (record->byte15 != 2) {
        if (func_0000F970(raw + tag, 1, 2, input) < 2) {
            /* This is the same 0x800AE0E8 message as the first read failure.
             * Distinct existing address forms keep KMC from combining the two
             * setup stubs before their shared reporting call. */
            failure_message = g_boot_resource_lzss_error_anchor - 0x1F18;
            goto report_read_error;
        }
    }
    D_800AF390 = raw + 2;
    record->byte00 = tag;
    func_00023460(raw + 2, record->bytes01, 5);
    D_800AF390 = raw + 7;
    record->word08 = func_0000BB44();
    record->word0C = func_0000BB44();
    record->word10 = func_0000BB44();
    record->byte14 = *D_800AF390++;
    record->byte15 = *D_800AF390++;
    if (record->byte15 != 2) {
        name_length = *D_800AF390++;
        for (count = 0; count < name_length; count++) {
            record->bytes16[count] = *D_800AF390++;
        }
        record->bytes16[name_length] = 0;
    } else {
        name_length = 0;
        record->word120 = record->word10;
    }
    record->half124 = 0x81B6;
    extension_length = tag - name_length;
    record->half128 = 0;
    record->half126 = 0;
    if (extension_length >= 24) {
        record->half116 = func_0000BB10();
        record->byte11C = *D_800AF390++;
        record->word118 = 1;
    } else if (extension_length == 22) {
        record->half116 = func_0000BB10();
        record->byte11C = 0;
        record->word118 = 1;
    } else {
        if (extension_length != 20) {
invalid_record:
            /* Separate zero returns let KMC merge this branch into the later
             * reporting tail and change the required annulled branch. */
            return 0;
        }
        record->byte11C = 0;
        record->word118 = 0;
    }

    if (record->byte11C == 'U' && record->byte15 == 0) {
        record->byte11D = *D_800AF390++;
        record->word120 = func_0000BB44();
        record->half124 = func_0000BB10();
        record->half126 = func_0000BB10();
        record->half128 = func_0000BB10();
        return 1;
    }
    if (record->byte15 != 0) {
        if (record->byte15 != 2) {
            D_800AF390 = raw + record->byte00;
        }
        extension_start = D_800AF390;
        prefix_base = prefix;
        /* Keep this real marker live across the dispatch calls. With the
         * shared reporting label, KMC otherwise rematerializes it in each arm
         * and drops the retail saved-register slot. */
        extension_marker = 'U';
        for (;;) {
            tag = func_0000BB10();
            if (tag == 0) {
                break;
            }
            if (record->byte15 != 2) {
                if (((int)raw - (int)D_800AF390) + (int)sizeof(raw) < tag ||
                    func_0000F970(D_800AF390, 1, tag, input) < (u32)tag) {
                    failure_message = D_800AE108;
report_read_error:
                    func_0000BFC0(failure_message);
                    goto invalid_record;
                }
            }
            switch (*D_800AF390++) {
            case 1:
                for (count = 0; count < tag - 3; count++) {
                    record->bytes16[count] = *D_800AF390++;
                }
                record->bytes16[tag - 3] = 0;
                break;
            case 2:
                {
                    int payload_length = tag - 3;
                    count = 0;
                    if (count < payload_length) {
                        u8 *destination = prefix_base;
                        do {
                            *destination++ = *D_800AF390++;
                            count++;
                        } while (count < payload_length);
                    }
                    prefix[tag - 3] = 0;
                }
                func_0000C938(prefix, '/');
                prefix_length = tag - 3;
                break;
            case 64:
                if ((record->byte11C == 'M') | (record->byte11C == 'H') || record->byte11C == 0) {
                    record->byte14 = func_0000BB10();
                }
                break;
            case 80:
                if (record->byte11C == extension_marker) {
                    record->half124 = func_0000BB10();
                }
                break;
            case 81:
                if (record->byte11C == extension_marker) {
                    record->half128 = func_0000BB10();
                    record->half126 = func_0000BB10();
                }
                break;
            case 84:
                if (record->byte11C == extension_marker) {
                    record->word120 = func_0000BB44();
                }
                break;
            /* These are the original table's 79 shared-handler entries. */
            case 0:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 20:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 41:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
            case 48:
            case 49:
            case 50:
            case 51:
            case 52:
            case 53:
            case 54:
            case 55:
            case 56:
            case 57:
            case 58:
            case 59:
            case 60:
            case 61:
            case 62:
            case 63:
            case 65:
            case 66:
            case 67:
            case 68:
            case 69:
            case 70:
            case 71:
            case 72:
            case 73:
            case 74:
            case 75:
            case 76:
            case 77:
            case 78:
            case 79:
            case 82:
            case 83:
            default:
                D_800AF390 = D_800AF390 + tag - 3;
                break;
            }
        }
        if (record->byte15 != 2) {
            int consumed = D_800AF390 - extension_start;
            if (consumed != 2) {
                u32 record_size = record->word08 + 2;
                int old_tag = record->byte00;
                record_size -= consumed;
                record->word08 = record_size;
                record->byte00 = old_tag + (consumed - 2);
            }
        }
    }
    if (prefix_length != 0) {
        strcat(prefix, record->bytes16);
        func_0000F450(record->bytes16, prefix);
        name_length += prefix_length;
    }
    switch (record->byte11C) {
    case 'M':
        func_0000BC3C(record->bytes16, name_length);
    case 'H':
        if (record->byte15 == 2) {
            record->word120 = record->word10;
        } else {
            record->word120 = 0;
        }
        break;
    case 'U':
        break;
    case 'm':
        func_0000BF48(record->bytes16, name_length);
        record->word120 = 0;
        break;
    default:
        func_0000BBA8(record->bytes16, name_length);
        if (record->byte15 == 2) {
            record->word120 = record->word10;
        } else {
            record->word120 = 0;
        }
        break;
    }
    return 1;
}

static u16 func_0000BB10(void)
{
    u32 first = *D_800AF390++;
    u32 second = *D_800AF390++;
    return (second << 8) | first;
}

static u32 func_0000BB44(void)
{
    u32 byte0 = *D_800AF390++;
    u32 byte1 = *D_800AF390++;
    u32 byte2 = *D_800AF390++;
    u32 byte3 = *D_800AF390++;
    return ((byte3 << 24) + (byte2 << 16) + (byte1 << 8)) | byte0;
}

static void func_0000BBA8(u8 *path, int length)
{
    u32 have_characters = length > 0;
    union { int scan_end; int slash; } phase;
    int contains_lowercase = 0;
    if (have_characters == 1) {
        u8 *scan = path;
        phase.scan_end = length + (int)path;
scan_character:
        if ((u32)(*scan - 'a') < 26) {
            contains_lowercase = 1;
            goto normalize_path;
        }
        scan++;
        if ((int)scan < phase.scan_end) {
            goto scan_character;
        }
    }
normalize_path:
    if (length > 0) {
        int backslash = '\\';
        phase.slash = '/';
        length += (int)path;
        do {
            int character = *path;
            if (character == backslash) {
                *path = phase.slash;
            } else if (!contains_lowercase && (u32)(character - 'A') < 26) {
                *path = character + 32;
            }
            path++;
        } while ((int)path < length);
    }
}

static void func_0000BC3C(u8 *path, int length)
{
    if (length > 0) {
        int backslash = '\\';
        int slash = '/';
        length += (int)path;
        do {
            int character = *path;
            if (character == backslash) {
                *path = slash;
            } else if ((u32)(character - 'A') < 26) {
                *path = character + 32;
            }
            path++;
        } while ((int)path < length);
    }
}
