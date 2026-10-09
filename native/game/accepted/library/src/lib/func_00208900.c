#include "game/combat_pose_pool.h"
#include "game/combat_pose_record.h"

typedef struct CombatBounds { int low04, low08, high04, high08; } CombatBounds;
typedef struct CombatImage {
    u8 field_00, field_01, field_02, field_03;
    u16 width, height;
} CombatImage;

extern int func_002015C8(int, int, int, int);
extern int func_0020161C(int, int, int, int);
extern CombatBounds func_00205378(int, int);
extern u32 func_00201E38(int, u32);
extern int func_00201E08(int, int, int);
extern void func_00208508(CombatImage *, int);
extern void *func_00206340(int, int, CombatPoseRecord *, int, int, int, u8 *);

void *func_00208900(int slot, int key, int variant, int directoryIndex,
                   int arg5, int arg6, int selected)
{
    CombatPoseScratch scratch;
    CombatBounds bounds;
    int resolved, handle;
    u32 index;
    CombatPosePoolRecord *record;
    u8 *slotBase;
    int width, height, stride, alphaStride, pixelBytes, bytes;
    CombatImage *output, *outputMask, *input, *maskBase, *inputMask;
    u8 *sourceRow, *maskRow, *destinationRow, *alphaRow;
    int sourceStride, maskStride, returnedMaskStride, y, x, dx, dy;
    u8 packed;
    int coverage;

    switch (key) {
    case 0x3B: resolved = 11; break;
    case 0x3C: resolved = 13; break;
    case 0x3D: resolved = 15; break;
    case 0x3E: resolved = 18; break;
    case 0x39: case 0x3F: case 0x40: resolved = 2; break;
    case 0x41: resolved = 3; break;
    case 0x42: resolved = 4; break;
    case 0x43: resolved = 5; break;
    case 0x44: resolved = 6; break;
    case 0x3A: case 0x46: resolved = 8; break;
    default: resolved = selected; break;
    }
    selected = resolved;
    handle = func_002015C8(key, variant, arg6, arg5);
    for (index = 0; index < 20; index++) {
        record = &D_801D0728[index];
        if (record->field_00 == handle &&
            func_0020161C(key, variant, arg6, arg5) == record->field_0C)
            break;
    }
    /* The original waits on this fixed failure condition without rescanning. */
    do {
    } while ((record == 0) | (index >= 20));
    /* Keep the indexed base separate from field_70: its lifetime and 32-bit
     * address addition preserve KMC's stack slot and operand order. */
    slotBase = (u8 *)((u32)slot * 4 + (u32)record);
    if (*(void **)(slotBase + 0x70)) {
        u8 *byteBase = (u8 *)record + slot;
        func_00208508(*(void **)(slotBase + 0x70), 15 - byteBase[0x98]);
    } else {
        bounds = func_00205378(handle, directoryIndex);
        width = bounds.high04 - bounds.low04;
        height = bounds.high08 - bounds.low08;
        stride = func_00201E38(2, width);
        pixelBytes = stride * height;
        alphaStride = func_00201E38(1, width);
        bytes = alphaStride * height + pixelBytes + 16;
        output = func_00001330(bytes);
        outputMask = (CombatImage *)((u8 *)output + pixelBytes + 8);
        func_00023780(output, bytes);
        output->field_00 = outputMask->field_00 = 0x36;
        output->field_01 = outputMask->field_01 = 0x34;
        output->field_02 = 0;
        outputMask->field_02 = 4;
        output->field_03 = 2;
        outputMask->field_03 = 1;
        output->width = outputMask->width = width;
        output->height = outputMask->height = height;
        *(void **)(slotBase + 0x70) = output;
        {
            u8 *byteBase = (u8 *)record + slot;
            byteBase[0x98] = 15;
        }
        index = 0;
        while (func_00205608(handle, directoryIndex, index, &scratch.record)) {
            input = func_00206340(key, variant, &scratch.record, arg5, arg6, selected, 0);
            maskBase = (CombatImage *)((u8 *)input +
                func_00201E08(input->field_03, input->width, input->height));
            sourceStride = func_00201E38(2, input->width);
            returnedMaskStride = func_00201E38((maskBase + 1)->field_03, (maskBase + 1)->width);
            sourceRow = (u8 *)(input + 1);
            maskRow = (u8 *)(maskBase + 2);
            y = 0;
            dy = scratch.record.field_08 - bounds.low08;
            dx = scratch.record.field_04 - bounds.low04;
            inputMask = maskBase + 1;
            /* Retain the returned stride after advancing to the second header. */
            maskStride = returnedMaskStride;
            destinationRow = (u8 *)output + (stride * dy + 8) + 2 * dx;
            alphaRow = (u8 *)outputMask + (alphaStride * dy + 8) + dx;
            for (; y < input->height; y++) {
                if (inputMask->field_03 == 1) {
                    for (x = 0; x < input->width; x++) {
                        if (maskRow[x]) {
                            alphaRow[x] = maskRow[x];
                            /* Retail reads and writes this destination halfword. */
                            ((volatile u16 *)destinationRow)[x] =
                                ((volatile u16 *)destinationRow)[x];
                        }
                    }
                    /* Branch-local row updates retain the retail allocation;
                     * KMC merges their common tail after register allocation. */
                    sourceRow += sourceStride;
                    maskRow += maskStride;
                    destinationRow += stride;
                    alphaRow += alphaStride;
                } else {
                    for (x = 0; x < input->width; x += 2) {
                        packed = maskRow[x / 2];
                        coverage = packed & 0xF0;
                        if ((u8)coverage) {
                            alphaRow[x] = coverage + ((u8)coverage >> 4);
                            ((u16 *)destinationRow)[x] = ((u16 *)sourceRow)[x];
                        }
                        coverage = packed & 15;
                        if (coverage) {
                            alphaRow[x + 1] = (coverage << 4) + coverage;
                            ((u16 *)destinationRow)[x + 1] = ((u16 *)sourceRow)[x + 1];
                        }
                    }
                    sourceRow += sourceStride;
                    maskRow += maskStride;
                    destinationRow += stride;
                    alphaRow += alphaStride;
                }
            }
            index++;
        }
    }
    return record->field_70[slot];
}
