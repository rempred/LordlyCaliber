typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct ResourceArchiveInput {
    u32 field_00;
    u32 field_04;
    u8 *field_08;
    u32 field_0C;
} ResourceArchiveInput;

/* Offsets describe the observed scratch storage. Record walks use the
 * complete object view so they preserve overlap with adjacent temporary fields. */
typedef struct ScenarioRecordScratch {
    u8 staged[0x28];
    u16 selected[4];
    u8 source_record[0x23];
    u8 gap_53[5];
    ResourceArchiveInput first;
    ResourceArchiveInput second;
    u8 gap_78[7];
    u8 apply_flag;
} ScenarioRecordScratch;

extern ResourceArchiveInput D_801D7C78;
extern u16 D_80187C42[][36];
extern u16 D_80187C44[][36];
extern u16 D_80187C46[][36];
extern u16 D_80187C48[][36];
extern void *func_0002DFB8(void *destination, u32 key);
extern u32 func_0002DEF4(u32 key);
extern void resource_free(void *resource);
extern void boot_resource_archive_load_many(ResourceArchiveInput *first,
                                            ResourceArchiveInput *second, s32 count);
extern void memcpy_aligned(void *destination, const void *source, s32 size);
extern void memset_00023780(void *destination, s32 size);
extern u8 func_000454c8(s32 selector);
extern u32 func_00249290(u16 slot, u16 *selected, s32 modifier, u32 option);
extern void func_002488D0(u8 *staged, u32 selector);

s32 func_00249A14(s32 record_index, s32 option, u8 apply_flag)
{
    ScenarioRecordScratch scratch;
    u32 *link;
    u32 inner_key;
    s32 group, member;
    s32 retained_option;
    u8 slot_count;

    scratch.first = D_801D7C78;
    scratch.second = D_801D7C78;
    scratch.apply_flag = apply_flag;
    retained_option = option;

    link = (u32 *)func_0002DFB8(0, 0x021AE74C);
    inner_key = *link;
    resource_free(link);
    scratch.first.field_0C = func_0002DEF4(inner_key);
    scratch.first.field_08 = (u8 *)func_0002DFB8(0, inner_key);
    scratch.second.field_04 = 0;
    boot_resource_archive_load_many(&scratch.first, &scratch.second, 1);
    group = 0;
    resource_free(scratch.first.field_08);

    memcpy_aligned(scratch.source_record,
                   scratch.second.field_08 + record_index * 0x23, 0x23);
    slot_count = 1;
    resource_free(scratch.second.field_08);
    memset_00023780(scratch.staged, 0x28);

    scratch.staged[0] = scratch.source_record[0];
    scratch.staged[2] = scratch.source_record[1];
    *(u16 *)(scratch.staged + 4) =
        (scratch.source_record[2] << 8) + scratch.source_record[3];
    *(u16 *)(scratch.staged + 6) =
        (scratch.source_record[4] << 8) + scratch.source_record[5];
    scratch.staged[1] = scratch.source_record[6];

    for (; (u8)group < 2; group++) {
        member = 0;
        while ((u8)member < 3) {
            if (scratch.source_record[13 + (u8)group * 9 + (u8)member] != 0) {
                u8 *slot = (u8 *)&scratch + ((u8)slot_count * 8);
                slot[0] = scratch.source_record[7 + (u8)group * 9];
                slot[2] = scratch.source_record[8 + (u8)group * 9];
                *(u16 *)(slot + 4) =
                    (scratch.source_record[9 + (u8)group * 9] << 8) +
                    scratch.source_record[10 + (u8)group * 9];
                *(u16 *)(slot + 6) =
                    (scratch.source_record[11 + (u8)group * 9] << 8) +
                    scratch.source_record[12 + (u8)group * 9];
                slot_count++;
                slot[1] = scratch.source_record[13 + (u8)group * 9 + (u8)member];
            }
            member++;
        }
    }

    /* Crossed reuse retains the two real counter lifetimes and loop layout. */
    member = 0;
    if ((u8)slot_count != 0) {
        do {
            memset_00023780(scratch.selected, 8);
            group = 0;
            while ((u8)group < 2) {
                u8 *slot = (u8 *)&scratch + ((u8)member * 8);
                u8 value = func_000454c8(*(u16 *)(slot + 4 + (u8)group * 2));
                u8 cls = slot[0];
                s32 match;
                u8 index;
                if (func_000454c8(D_80187C42[cls][0]) == value) {
                    match = 1;
                } else if (func_000454c8(D_80187C44[cls][0]) == value) {
                    match = 2;
                } else if (func_000454c8(D_80187C46[cls][0]) == value) {
                    match = 3;
                } else {
                    /* Keep the final probe in the word-sized result before
                     * narrowing the selected index. This preserves the pinned
                     * compiler's branch and delay-slot ordering. */
                    match = func_000454c8(D_80187C48[cls][0]);
                    if (match == value) {
                        match = 4;
                    } else {
                        match = 0;
                    }
                }
                index = match;
                if (index != 0) {
                    scratch.selected[index - 1] = *(u16 *)(slot + 4 + (u8)group * 2);
                }
                group++;
            }
            {
                u8 *slot = (u8 *)&scratch + ((u8)member * 8);
                slot[0] = func_00249290(slot[0], scratch.selected,
                                      (s8)slot[2], (u8)retained_option);
            }
            member++;
        } while ((u8)member < (u8)slot_count);
    }

    if (scratch.apply_flag != 0) {
        func_002488D0(scratch.staged, 0xFF);
    }
    return 0;
}
