typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct ResourceRow {
    u8 bytes[0x34];
} ResourceRow;

typedef struct ResourceArchiveInput {
    u32 field_00;
    u32 field_04;
    u8 *field_08;
    u32 field_0C;
} ResourceArchiveInput;

extern ResourceArchiveInput D_801D16B0;
extern u8 D_80193670, D_80193671, D_80193672;
extern u8 D_801936A8;
extern u16 D_80197B60;
extern u8 D_801976E8, D_801976DC, D_801976D8;
extern u8 D_801971F0[];
extern u8 D_80193BD3[][0x38];
extern ResourceRow D_80195560[];
extern u8 D_80195593[];
extern u8 D_801953F0[];
extern u16 D_80195576;
extern u16 D_801953F2;
extern u16 D_801953F4[];
extern u8 D_801954E4[];
extern u8 D_80195594[];
extern u8 D_801955A5;
extern u8 D_801974DE[];

extern void *func_0002DFB8(void *destination, u32 key);
extern u32 func_0002DEF4(u32 key);
extern void resource_free(void *resource);
extern void boot_resource_archive_load_many(ResourceArchiveInput *first,
                                            ResourceArchiveInput *second, s32 count);
extern void memcpy_aligned(void *destination, const void *source, s32 count);
extern void memset_00023780(void *destination, s32 count);
extern void func_0023A3A0(u8 *row, u32 kind, s32 adjustment,
                          u32 field_a, u32 field_b);
extern void func_00048294(void);
extern void func_0004813C(s32 value);
extern void func_000466f4(void);


/* Every optional source count constructs another output index. */
#define RESOLVE_SOURCE(group, kind, limit, result) {            \
    s32 adjustment; \
    u32 field_a, field_b; \
    u32 high_b, low_a, low_b; \
    u32 shifted_a, shifted_b; \
    s32 kind_value; \
    high_a = (group)[2]; \
    delta = (s8)(group)[1]; \
    high_b = (group)[4]; \
    low_a = (group)[3]; \
    low_b = (group)[5]; \
    kind_value = (kind); \
    shifted_a = high_a << 8; \
    shifted_b = high_b << 8; \
    adjustment = delta + (limit); \
    field_b = shifted_b + low_b; \
    field_a = shifted_a + low_a; \
    if (kind_value == 1) {                                                      \
        u32 free_index = 0;                                                 \
        while (free_index < 0x78 && (D_801953F0[0xF4 + free_index] & 1)) {         \
            free_index++;                                                   \
        }                                                                   \
        *(u16 *)(D_801953F0 + 4 + free_index * 2) = D_801953F2;                               \
        D_801953F0[0xF4 + free_index] = 1;                                        \
        (result) = (s32)free_index + 0x64;                                  \
    } else {                                                                \
        ResourceRow *row = &D_80195560[1];                                   \
        if (D_801955A5 != 0) {                                              \
            do { row++; } while (row->bytes[0x11] != 0);                     \
        }                                                                   \
        func_0023A3A0(row->bytes, kind_value, adjustment,                 \
                      field_a, field_b);                                   \
        (result) = (s32)(row - D_80195560);                                  \
    }                                                                       \
}

#define EMIT_OPTION(group, kind, field, limit, output_count) {           \
    if ((group)[field] != 0) {                                               \
        s32 selected_index;                                                 \
        RESOLVE_SOURCE((group), (kind), (limit), selected_index);         \
        *(output + (output_count) + 2) = (u8)selected_index;               \
        *(output + (output_count) + 7) = (u8)((group)[field] - 1);          \
        (output_count)++;                                                   \
    }                                                                       \
}

void func_0023A5EC(void)
{
    u8 record[0x23];
    ResourceArchiveInput first, second;
    u32 *link;
    u32 inner_key;
    s32 record_index;
    s32 limit;
    s32 maximum;
    s32 selected_index;
    u32 i;
    u8 *group;
    u8 *scan_row;
    u8 selector = D_80193672;
    u8 *state;
    u8 *output = D_801971F0 + 0x2EE;
    /* Share these actual byte values across constructor sites. GCC 2.7.2
     * otherwise swaps the adjustment and low-byte temporaries. Keep each
     * site's kind, shifts and sums single-set to preserve the schedule. */
    s32 delta;
    u32 high_a;

    record_index = (D_80193670 << 8) | D_80193671;
    limit = 0x63;
    if (D_80197B60 != 0) {
        record_index = D_80197B60 & 0x7FFF;
    }
    state = &D_801976E8;
    if ((*state) == 0x1E) {
        return;
    }

    first = D_801D16B0;
    second = D_801D16B0;
    link = (u32 *)func_0002DFB8(0, 0x021AE74C);
    inner_key = *link;
    resource_free(link);
    first.field_0C = func_0002DEF4(inner_key);
    first.field_08 = (u8 *)func_0002DFB8(0, inner_key);
    second.field_04 = 0;
    boot_resource_archive_load_many(&first, &second, 1);
    resource_free(first.field_08);
    memcpy_aligned(record, second.field_08 + record_index * 0x23, 0x23);
    resource_free(second.field_08);
    group = record;

    (*state) = 0x1E;
    maximum = 0;
    scan_row = &D_801971F0[D_801976DC * 0x19];
    for (i = 0; i < 5; i++) {
        u8 value = *(scan_row + i + 2);
        if (value != 0 && value < 0x64) {
            u8 candidate = D_80193BD3[value][0];
            if (maximum < candidate) {
                maximum = candidate;
            }
        }
    }
    if ((u32)maximum < (u32)limit) {
        limit = maximum;
    }

    i = 0;
    memset_00023780(D_80195560, 0x141C);
    memset_00023780(D_801953F0, 0x16C);
    D_801953F0[0] = 0x78;
    D_801953F0[1] = 0x78;
    func_0023A3A0(D_80195560[0].bytes, 1, limit, 0, 0);
    D_801953F2 = D_80195576;
    memset_00023780(output, 0x19);

    RESOLVE_SOURCE(group, group[0], limit, selected_index);
    *(output + i + 2) = (u8)selected_index;
    D_80195593[(u8)selected_index * 0x34] = 2;
    output[1] = 0x81;
    output[0xC] = selector;
    *(output + i + 7) = (u8)(group[6] - 1);

    i++;
    group += 7;
    if (group[0] != 0 && (group[6] != 0 || group[7] != 0 || group[8] != 0)) {
        EMIT_OPTION(group, group[0], 6, limit, i);
        EMIT_OPTION(group, group[0], 7, limit, i);
        EMIT_OPTION(group, group[0], 8, limit, i);
    }
    group += 9;
    if (group[0] != 0 && (group[6] != 0 || group[7] != 0 || group[8] != 0)) {
        EMIT_OPTION(group, group[0], 6, limit, i);
        EMIT_OPTION(group, group[0], 7, limit, i);
        EMIT_OPTION(group, group[0], 8, limit, i);
    }

    if (D_801976D8 & 8) {
        D_801936A8 = 0x44;
        func_00048294();
        func_0004813C(8);
        func_000466f4();
    }
}

#undef EMIT_OPTION
#undef RESOLVE_SOURCE
