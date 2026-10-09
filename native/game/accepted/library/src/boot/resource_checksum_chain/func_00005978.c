#include "interfaces.h"

/* Complete 5978..5A88; the global-pointer read precedes the 5980 prologue. */
s32 func_00005978(void *record)
{
    ProbeRecordPrefix *prefix = (ProbeRecordPrefix *)record;
    const u8 *payload = (const u8 *)prefix + 0x0C;
    s32 offset;
    /* Initialize the real count before setup calls: this pinned compiler
     * otherwise rematerializes it separately at the two checksum calls. */
    u16 count = 0x4ADC;

    if (D_800A83B8 == 0) {
        D_800A83B8 = resource_alloc(0x8000);
        offset = 0;
        do {
            func_0001A4F0(offset, D_800A83B8 + offset, 0x100, 0);
            offset += 0x100;
        } while (offset < 0x8000);
    }
    func_00023460(D_800A83B8 + 0x30B0, prefix, 0x4AE8);
    if (func_00023350(D_800A8240, prefix->bytes_04, 8) != 0) {
        return 0;
    }

    /* Retail reads all four bytes here before the two separate halfwords. */
    if (*(const u32 *)prefix != 0) {
        if (prefix->word_00 != (u16)func_00005E84(payload, count, 0x30B0)) {
            return 0;
        }
        if (prefix->word_02 != (u16)func_00005F00(payload, count, 0x30B0)) {
            return 0;
        }
    }
    return 1;
}
