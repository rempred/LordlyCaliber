#include "common/types.h"

typedef void (*ProbeCallback)(void *);

extern void *func_00001330(u32 bytes);
extern void func_000016C4(void *record);
extern void func_00005624(void);
extern void func_000050F0(void *record, u32 offset, u32 bytes);
extern void func_8016CDCC(void *record);
extern void func_00004FF0(u32 key);
/* Separate fields in the retained table, whose entries are 0x1C bytes apart. */
extern u8 D_800A8250[];
extern u8 D_800A8258[];

void boot_resource_probe_dispatch_apply(s32 id)
{
    u8 *small;
    u8 *record;
    s32 count;
    s32 tableOffset;
    ProbeCallback callback;

    if (id == 15) {
        func_00005624();
    } else if (id == 14) {
        small = func_00001330(0x10);
        func_000050F0(small, 0, 0x10);
        func_8016CDCC(small + 0xC);
        func_000016C4(small);
    } else {
        record = func_00001330(0x1850);
        func_000050F0(record, id * 0x1850 + 0x10, 0x1850);
        count = 0;
        tableOffset = 0;
        do {
            callback = *(ProbeCallback *)(D_800A8250 + tableOffset);
            if (callback != 0) {
                callback(record + (*(s32 *)(D_800A8258 + tableOffset) + 0xC));
            }
            count++;
            tableOffset += 0x1C;
        } while (count < 13);
        func_000016C4(record);
    }
    func_00004FF0(0x37081383);
}
