#include "game/scenario_source_record.h"

extern void *func_80070F30(s32);
extern void func_800712C4(void *);
extern void func_80093380(void *, s32);
extern u32 func_0014F300(s32);
extern void func_00130E60(u16, s32);

/* The accepted direct callers pass byte-valued arguments and ignore the result.
 * This private declaration does not establish the original formal return type. */
void func_00195D9C(u8 index, u8 preset0, u8 preset1, u8 preset2)
{
    u8 *buffer;
    s32 position;
    s32 primaryValue;
    u8 *destination;
    s32 capacity;
    s32 count;
    s32 phase;
    s32 kind;
    s32 scanIndex;
    s32 secondaryValue;
    u32 sourceIndex;

    buffer = func_80070F30(20);
    func_80093380(buffer, 20);
    /* The accepted helper returns a word; retail consumes only its low16 bits. */
    capacity = (u16)func_0014F300(index);

    /* Keep both byte values available across the joined construction entry.
     * The separate count definitions and joined phase guard are significant
     * to KMC's CSE/COMBINE passes and preserve its otherwise unused frame slot. */
    if (((primaryValue = preset0) != 0) |
        ((secondaryValue = preset1) != 0)) {
        count = 0;
        goto construction_entry;
    }
    if (preset2) {
        count = 0;
construction_entry:
        phase = 0;
        if (phase < capacity) {
            /* A known-zero write position lets KMC derive the moving cursor.
             * Keep the logical count updates beside each write: its allocator
             * weighs these uses before COMBINE merges adjacent increments. */
            position = 0;
            do {
                switch (phase) {
                case 0:
                    switch ((u8)primaryValue) {
                    case 1:
                        buffer[position++] = 1;
                        position += 4;
                        count += 5;
                        break;
                    case 2:
                        buffer[position++] = 1;
                        count++;
                        buffer[position++] = 1;
                        position += 2;
                        count += 3;
                        break;
                    case 3:
                        buffer[position++] = 1;
                        count++;
                        buffer[position++] = 1;
                        count++;
                        buffer[position++] = 2;
                        position += 3;
                        count += 4;
                        break;
                    case 4:
                        buffer[position++] = 2;
                        count++;
                        buffer[position++] = 2;
                        position += 2;
                        count += 3;
                        break;
                    case 5:
                        buffer[position++] = 2;
                        count++;
                        buffer[position++] = 2;
                        count++;
                        buffer[position++] = 3;
                        position += 3;
                        count += 4;
                        break;
                    case 6:
                        buffer[position++] = 3;
                        count++;
                        buffer[position++] = 3;
                        position += 2;
                        count += 3;
                        break;
                    }
                    phase = 1;
                    break;
                case 1:
                    /* This is a byte conversion. KMC combines the shift pair
                     * through the selector copy; a u8 cast leaves an extra
                     * mask. The s32 result retains signed switch comparisons. */
                    switch ((s32)(((u32)secondaryValue << 24) >> 24)) {
                    case 1:
                        buffer[position++] = 4;
                        count++;
                        buffer[position++] = 4;
                        count++;
                        break;
                    case 2:
                        buffer[position++] = 4;
                        count++;
                        buffer[position++] = 5;
                        count++;
                        break;
                    case 3:
                        buffer[position++] = 5;
                        count++;
                        buffer[position++] = 5;
                        count++;
                        break;
                    }
                    phase = 2;
                    break;
                case 2:
                    switch (preset2) {
                    case 1:
                        buffer[position++] = 6;
                        count++;
                        break;
                    case 2:
                        buffer[position++] = 6;
                        count++;
                        buffer[position++] = 6;
                        count++;
                        break;
                    }
                    phase = 0;
                    break;
                }
            } while (count < capacity);
        }
    }

    sourceIndex = index;
    destination = g_func_001957D0_source_records[sourceIndex].field_0D;
    func_80093380(destination, 10);
    if (preset0 || preset1 || preset2) {
        kind = 1;
        scanIndex = 0;
        count = 0;
        while (count < capacity && kind < 7) {
            if (scanIndex >= capacity) {
                kind++;
                scanIndex = 0;
            }
            if (buffer[scanIndex] == kind) {
                destination[count++] = kind;
                if (sourceIndex < 30)
                    func_00130E60(kind, 1);
            }
            scanIndex++;
        }
    }
    func_800712C4(buffer);
}
