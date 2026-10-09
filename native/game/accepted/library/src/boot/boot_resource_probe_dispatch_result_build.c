#include "common/types.h"

typedef struct ProbeRecord {
    u32 field00;
    u32 field04;
    u32 field08;
    u32 field0C;
} ProbeRecord;

extern void *func_00001330(u32 bytes);
extern void func_000016C4(void *record);
extern s32 func_00005978(void *record);
extern s32 func_0000581C(s32 id, void *record);
extern void func_000050F0(void *record, u32 offset, u32 bytes);
extern void func_00004FF0(u32 key);
extern void func_00023460(const void *source, void *destination, u32 bytes);
extern u8 D_800A8258[];

void *boot_resource_probe_dispatch_result_build(s32 id)
{
    u8 *result = 0;
    u8 *source = 0;
    ProbeRecord *record;
    u32 marker;

    if (id == 15) {
        record = func_00001330(0x4AE8);
        if (!func_00005978(record)) {
            goto cleanup;
        }
        func_000050F0(record, 0x30B0, 0x4AE8);
        marker = record->field00;
    } else {
        record = func_00001330(0x1850);
        if (!func_0000581C(id, record)) {
            goto cleanup;
        }
        func_000050F0(record, id * 0x1850 + 0x10, 0x1850);
        marker = record->field0C;
    }
    if (marker != 0) {
        source = (u8 *)record + (*(s32 *)D_800A8258 + 0xC);
    }

cleanup:
    func_00004FF0(0x37081383);
    if (source != 0) {
        result = func_00001330(0x1A);
        func_00023460(source, result, 0x1A);
    }
    func_000016C4(record);
    return result;
}
