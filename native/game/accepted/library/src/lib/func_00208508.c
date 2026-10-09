#include "game/combat_types.h"

typedef unsigned char u8;
typedef struct CombatImage {
    u8 field_00, field_01, field_02, field_03;
    u16 width, height;
} CombatImage;

extern int func_00201E08(int, int, int);
extern u32 func_00201E38(int, u32);
extern void *func_00001330(u32);
extern void func_00023780(void *, u32);
extern void func_000016C4(void *);
extern void memcpy(void *source, void *destination, u32 bytes);
extern float sqrtf(float);

void func_00208508(CombatImage *input, int parameter)
{
    CombatImage *inputMask;
    int stride, maskStride;
    u16 *source;
    u8 *sourceMask;
    int halfStride;
    CombatImage *output, *outputMask;
    int y, x, centerX, centerY, dx, dy;
    int sampleX, sampleY;
    u16 *destination;
    u8 *destinationMask;
    float scale, offsetX, offsetY, scaleDivisor;

    inputMask = (CombatImage *)((u8 *)input +
        (func_00201E08(input->field_03, input->width, input->height) + 8));
    stride = func_00201E38(input->field_03, input->width);
    maskStride = func_00201E38(inputMask->field_03, inputMask->width);
    /* Separate size lifetimes preserve KMC's allocation and copy registers. */
    {
        int bytes = maskStride * inputMask->height;
        bytes += 16;
        output = func_00001330(stride * input->height + bytes);
    }
    {
        int bytes = maskStride * inputMask->height;
        bytes += 16;
        outputMask = (CombatImage *)((u8 *)output + (stride * input->height + 8));
        func_00023780(output, stride * input->height + bytes);
    }
    output->field_00 = outputMask->field_00 = 0x36;
    output->field_01 = outputMask->field_01 = 0x34;
    output->field_02 = 0;
    outputMask->field_02 = 4;
    output->field_03 = 2;
    outputMask->field_03 = 1;
    output->width = outputMask->width = input->width;
    output->height = outputMask->height = input->height;
    centerX = input->width >> 1;
    centerY = input->height >> 1;
    source = (u16 *)(input + 1);
    sourceMask = (u8 *)(inputMask + 1);
    for (y = 0; y < input->height; y++) {
        /* Keep the scale constant ahead of the signed half-stride calculation. */
        scaleDivisor = 100.0f;
        halfStride = stride / 2;
        destination = (u16 *)((u8 *)(output + 1) + y * stride);
        destinationMask = (u8 *)(outputMask + 1) + y * maskStride;
        for (x = 0; x < input->width; x++) {
            if (x == centerX && y == centerY) {
                dx = 1;
                dy = 1;
            } else {
                dx = x - centerX;
                dy = y - centerY;
            }
            /* The explicit builtin preserves KMC's inline sqrt and fallback call. */
            scale = (float)parameter + (float)(parameter * (input->width - parameter)) /
                __builtin_sqrtf((float)(dx * dx + dy * dy));
            /* The two axes cross in the original transform. */
            offsetX = (float)dy * scale / scaleDivisor;
            offsetY = (float)dx * scale / scaleDivisor;
            sampleX = (int)((float)x + offsetX);
            sampleY = (int)((float)y - offsetY);
            if (sampleX >= 0 && sampleX < input->width &&
                sampleY >= 0 && sampleY < input->height) {
                destination[x] = source[halfStride * sampleY + sampleX];
                destinationMask[x] = sourceMask[maskStride * sampleY + sampleX];
            }
        }
    }
    {
        int bytes = maskStride * inputMask->height;
        bytes += 16;
        memcpy(output, input, stride * input->height + bytes);
    }
    func_000016C4(output);
}

