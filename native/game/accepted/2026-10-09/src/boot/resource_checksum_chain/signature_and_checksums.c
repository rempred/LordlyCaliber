#include "interfaces.h"

void func_00005D9C(s32 id, void *record)
{
    /* Use the incoming pointer directly so no second user-variable alias
     * survives through the selector paths in this compiler. */
    /* Keep the two real payload definitions and each real signature copy
     * in its selector path. A common copy places the word_02 store before
     * its argument setup instead of in the call delay slot. */
    const u8 *payload;

    if (id == 0x0E) {
        payload = (const u8 *)record + 0x0C;
        ((ProbeRecordPrefix *)record)->word_00 = func_00005E84(payload, 4, 0);
        ((ProbeRecordPrefix *)record)->word_02 = func_00005F00(payload, 4, 0);
        func_00023460(D_800A8240, ((ProbeRecordPrefix *)record)->bytes_04, 8);
    } else {
        payload = (const u8 *)record + 0x0C;
        if (id == 0x0F) {
            ((ProbeRecordPrefix *)record)->word_00 = func_00005E84(payload, 0x4ADC, 0x30B0);
            ((ProbeRecordPrefix *)record)->word_02 = func_00005F00(payload, 0x4ADC, 0x30B0);
            func_00023460(D_800A8240, ((ProbeRecordPrefix *)record)->bytes_04, 8);
        } else {
            /* Unsigned arithmetic retains the observed low-word shift/add value. */
            u32 seed = (u32)id * 0x1850U + 0x10U;
            ((ProbeRecordPrefix *)record)->word_00 = func_00005E84(payload, 0x1844, seed);
            ((ProbeRecordPrefix *)record)->word_02 = func_00005F00(payload, 0x1844, seed);
            func_00023460(D_800A8240, ((ProbeRecordPrefix *)record)->bytes_04, 8);
        }
    }
}

u32 func_00005E84(const u8 *bytes, u32 count, u32 seed)
{
    u32 total = 0;
    u32 index = 0;

    count &= 0xFFFFU;
    if (count != 0) {
        do {
            total += bytes[index & 0xFFFFU];
            index++;
        } while ((index & 0xFFFFU) < count);
    }
    return (total + seed) & 0xFFFFU;
}

u32 func_00005EC4(const u8 *bytes, u32 count)
{
    u32 total = 0;
    u32 index = 0;

    count &= 0xFFFFU;
    if (count != 0) {
        do {
            total += bytes[index & 0xFFFFU];
            index++;
        } while ((index & 0xFFFFU) < count);
    }
    return total & 0xFFFFU;
}

u16 func_00005F00(const u8 *bytes, u16 count, u32 seed)
{
    /* The halfword total retains the conditional increment in this compiler.
     * Modulo accumulation preserves the observed low-halfword result. */
    u16 total = 0;
    u16 index = 0;

    if (count != 0) {
        do {
            u32 value = bytes[index];
            u16 bit;

            for (bit = 0; bit < 8; bit++) {
                if (value & 1) total++;
                value >>= 1;
            }
            index++;
        } while (index < count);
    }
    return (u16)(total + seed);
}

u16 func_00005F60(const u8 *bytes, u16 count)
{
    /* The halfword total retains the conditional increment in this compiler.
     * Modulo accumulation preserves the observed low-halfword result. */
    u16 total = 0;
    u16 index = 0;

    if (count != 0) {
        do {
            u32 value = bytes[index];
            u16 bit;

            for (bit = 0; bit < 8; bit++) {
                if (value & 1) total++;
                value >>= 1;
            }
            index++;
        } while (index < count);
    }
    return total;
}
