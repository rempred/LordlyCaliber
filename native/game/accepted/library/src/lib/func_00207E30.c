#include "game/combat_pose_record.h"

typedef unsigned char u8;
typedef struct CombatBounds { int low04, low08, high04, high08; } CombatBounds;
typedef struct CombatImage { u8 field_00, field_01, field_02, field_03; u16 width, height; } CombatImage;

extern int func_002015C8(int, int, int, int);
extern CombatBounds func_00205378(int, int);
extern u32 func_00201E38(int, u32);
extern int func_00201E08(int, int, int);
extern void *func_00001330(u32);
extern void func_00023780(void *, u32);
extern void *func_00206340(int, int, CombatPoseRecord *, int, int, int, u8 *);

#define BLEND_PIXEL(n) \
    ((u16 *)destinationRow)[n] = (((((u16 *)destinationRow)[n] & 0xF800) * (255 - (int)alpha) / 255 + \
                       (((u16 *)sourceRow)[n] & 0xF800) * (int)alpha / 255) & 0xF800) | \
                     (((((u16 *)destinationRow)[n] & 0x07C0) * (255 - (int)alpha) / 255 + \
                       (((u16 *)sourceRow)[n] & 0x07C0) * (int)alpha / 255) & 0x07C0) | \
                     (((((u16 *)destinationRow)[n] & 0x003E) * (255 - (int)alpha) / 255 + \
                       (((u16 *)sourceRow)[n] & 0x003E) * (int)alpha / 255) & 0x003E); \
    coverage = alphaRow[n] + (int)alpha; \
    if (coverage > 255) coverage = 255; \
    alphaRow[n] = coverage

void *func_00207E30(int a, int b, int directoryIndex, int d, int c,
                   int variant, CombatBounds *bounds)
{
    CombatPoseScratch scratch;
    CombatBounds localBounds;
    int resolvedVariant, width, height, bytes, pixelBytes;
    CombatImage *input, *maskBase;
    u16 *sourceRow;
    u8 *maskRow, *destinationRow, *alphaRow;
    int stride, alphaStride, index;
    CombatImage *output, *outputMask;
    int handle;
    int maskStride;
    CombatImage *inputMask;
    int y, sourceStride;
    int x, dx, dy;
    u8 packed;
    int coverage;
    u32 alpha, expanded;

    switch (a) {
    case 0x3B: resolvedVariant = 11; break;
    case 0x3C: resolvedVariant = 13; break;
    case 0x3D: resolvedVariant = 15; break;
    case 0x3E: resolvedVariant = 18; break;
    case 0x39: case 0x3F: case 0x40: resolvedVariant = 2; break;
    case 0x41: resolvedVariant = 3; break;
    case 0x42: resolvedVariant = 4; break;
    case 0x43: resolvedVariant = 5; break;
    case 0x44: resolvedVariant = 6; break;
    case 0x3A: case 0x46: resolvedVariant = 8; break;
    default: resolvedVariant = variant; break;
    }
    variant = resolvedVariant;
    handle = func_002015C8(a, b, c, d);
    localBounds = func_00205378(handle, directoryIndex);
    *bounds = localBounds;
    width = bounds->high04 - bounds->low04;
    height = bounds->high08 - bounds->low08;
    stride = func_00201E38(2, width);
    pixelBytes = stride * height;
    alphaStride = func_00201E38(1, width);
    bytes = alphaStride * height + pixelBytes + 16;
    index = 0;
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

    while (func_00205608(handle, directoryIndex, index, &scratch.record)) {
        input = func_00206340(a, b, &scratch.record, d, c, variant, 0);
        sourceRow = (u16 *)(input + 1);
        maskBase = (CombatImage *)((u8 *)input +
                    func_00201E08(input->field_03, input->width, input->height));
        inputMask = maskBase + 1;
        sourceStride = func_00201E38(2, input->width);
        maskStride = func_00201E38(inputMask->field_03, inputMask->width);
        y = 0;
        dy = scratch.record.field_08 - bounds->low08;
        dx = scratch.record.field_04 - bounds->low04;
        destinationRow = (u8 *)(output + 1) +
            stride * dy + 2 * dx;
        alphaRow = (u8 *)(outputMask + 1) +
            alphaStride * dy + dx;
        maskRow = (u8 *)(maskBase + 2);
        while (y < input->height) {
            if (inputMask->field_03 == 1) {
                for (x = 0; x < input->width; x++) {
                    if (maskRow[x]) {
                        alphaRow[x] = maskRow[x];
                        /* Preserve the retail read/write even when the pixel is unchanged. */
                        ((u16 *)destinationRow)[x] = ((u16 *)destinationRow)[x];
                    }
                }
                /* Keep each format's four row updates together. KMC's later
                 * shared-tail merge retains the retail register allocation. */
                sourceRow = (u16 *)((u8 *)sourceRow + sourceStride);
                destinationRow += stride;
                maskRow += maskStride;
                alphaRow += alphaStride;
            } else {
                for (x = 0; x < input->width; x += 2) {
                    packed = *(volatile u8 *)(maskRow + x / 2);
                    coverage = (signed char)packed & 0xF0;
                    alpha = (u8)coverage;
                    if (alpha) {
                        alpha >>= 4;
                        expanded = coverage | alpha;
                        alpha = expanded;
                        BLEND_PIXEL(x);
                    }
                    coverage = packed & 0xF;
                    if (coverage) {
                        alpha = (u8)(coverage << 4);
                        alpha |= coverage;
                        BLEND_PIXEL(x + 1);
                    }
                }
                sourceRow = (u16 *)((u8 *)sourceRow + sourceStride);
                destinationRow += stride;
                maskRow += maskStride;
                alphaRow += alphaStride;
            }
            y++;
        }
        index++;
    }
    return output;
}

#undef BLEND_PIXEL
