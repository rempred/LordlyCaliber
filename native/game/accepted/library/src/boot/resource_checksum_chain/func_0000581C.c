#include "interfaces.h"

/* Complete original body 581C..5978; 588C is the chunk-loop target. */
s32 func_0000581C(s32 id, void *record)
{
    ProbeRecordPrefix *prefix = (ProbeRecordPrefix *)record;
    const u8 *payload = (const u8 *)prefix + 0x0C;
    u32 recordOffset = (u32)id * 0x1850U + 0x10U;
    u32 seed;
    /* Initialize the real count before setup calls: this pinned compiler
     * otherwise rematerializes it separately at the two checksum calls. */
    u16 count = 0x1844;
    s32 offset;

    if (D_800A83B8 == 0) {
        D_800A83B8 = resource_alloc(0x8000);
        offset = 0;
        do {
            func_0001A4F0(offset, D_800A83B8 + offset, 0x100, 0);
            offset += 0x100;
        } while (offset < 0x8000);
    }
    func_00023460(D_800A83B8 + recordOffset, prefix, 0x1850);
    if (func_00023350(D_800A8240, prefix->bytes_04, 8) != 0) {
        goto mismatch;
    }

    seed = (u32)id * 0x1850U + 0x10U;
    if (prefix->word_00 != (u16)func_00005E84(payload, count, seed)) {
        goto mismatch;
    }
    if (prefix->word_02 != (u16)func_00005F00(payload, count, seed)) {
        goto mismatch;
    }
    return 1;

mismatch:
    return 0;
}
