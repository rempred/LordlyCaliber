#include "interfaces.h"

/* Complete 5A88..5B8C; the global-pointer read precedes the 5A90 prologue. */
s32 func_00005A88(void *record)
{
    ProbeRecordPrefix *prefix = (ProbeRecordPrefix *)record;
    const u8 *payload = (const u8 *)prefix + 0x0C;
    s32 offset;
    /* Initialize the real count before setup calls: this pinned compiler
     * otherwise rematerializes it separately at the two checksum calls. */
    u16 count = 4;

    if (D_800A83B8 == 0) {
        D_800A83B8 = resource_alloc(0x8000);
        offset = 0;
        do {
            func_0001A4F0(offset, D_800A83B8 + offset, 0x100, 0);
            offset += 0x100;
        } while (offset < 0x8000);
    }
    func_00023460(D_800A83B8, prefix, 0x10);
    if (func_00023350(D_800A8240, prefix->bytes_04, 8) != 0) {
        goto mismatch;
    }
    if (prefix->word_00 != (u16)func_00005E84(payload, count, 0)) {
        goto mismatch;
    }
    if (prefix->word_02 != (u16)func_00005F00(payload, count, 0)) {
        goto mismatch;
    }
    return 1;

mismatch:
    return 0;
}
