#include "common/types.h"

typedef void (*ProbeCallback)(void *);

typedef struct ProbeRecordPrefix {
    u32 field00;
} ProbeRecordPrefix;

extern void *resource_alloc(u32 bytes);
extern void resource_free(void *record);
extern void func_0001A4F0(s32 offset, void *destination, u32 bytes, s32 mode);
extern void func_00023460(const void *source, void *destination, u32 bytes);
extern u8 *D_800A83B8;
/* Separate observed fields in the retained table, at 0x1C-byte entry stride. */
extern u8 D_800A8250[];
extern u8 D_800A8258[];
extern u8 D_800A825C[];
extern u8 D_800A8264[];

void boot_resource_probe_global_buffer_dual_callback_apply(void)
{
    ProbeRecordPrefix *record;
    s32 offset;
    s32 count;
    s32 tableOffset;
    ProbeCallback callback;

    record = resource_alloc(0x4AE8);
    if (D_800A83B8 == 0) {
        D_800A83B8 = resource_alloc(0x8000);
        offset = 0;
        do {
            func_0001A4F0(offset, D_800A83B8 + offset, 0x100, 0);
            offset += 0x100;
        } while (offset < 0x8000);
    }
    func_00023460(D_800A83B8 + 0x30B0, record, 0x4AE8);
    if (record->field00 != 0) {
        count = 0;
        tableOffset = 0;
        do {
            callback = *(ProbeCallback *)(D_800A8250 + tableOffset);
            if (callback != 0) {
                callback((u8 *)record + (*(s32 *)(D_800A8258 + tableOffset) + 0xC));
            }
            count++;
            tableOffset += 0x1C;
        } while (count < 13);

        count = 0;
        tableOffset = 0;
        do {
            callback = *(ProbeCallback *)(D_800A825C + tableOffset);
            if (callback != 0) {
                callback((u8 *)record + (*(s32 *)(D_800A8264 + tableOffset) + 0x1850));
            }
            count++;
            tableOffset += 0x1C;
        } while (count < 13);
    }
    resource_free(record);
}
