typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef s32 M2C_UNK;
typedef s8 M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;
s32 func_00045ae0(s32, u8, u16 **);
void resource_free(s32);
extern u8 * volatile D_80196AF8;

void func_000c6bec(s32 arg0, s32 arg1) {
    u16 *loaded_pixels;
    u16 *pixels;
    u8 *data;
    register u8 *early_data asm("$3");
    u16 active_index;
    s32 resource;
    u32 index;

    early_data = D_80196AF8;
    active_index = *(u16 *)(early_data + 0x24EC);
    resource = *(s32 *)(early_data + (active_index * 4) + 0x68);
    if (resource != 0) {
        resource_free(resource);
        early_data = D_80196AF8;
        active_index = *(u16 *)(early_data + 0x24EC);
        *(s32 *)(early_data + (active_index * 4) + 0x68) = 0;
    }

    resource = func_00045ae0(arg0 & 0xFF, *(u8 *)0x801939D3, &loaded_pixels);
    data = D_80196AF8;
    active_index = *(u16 *)(data + 0x24EC);
    *(s32 *)(data + (active_index * 4) + 0x68) = resource;
    active_index = *(u16 *)(data + 0x24EC);
    asm volatile(
        "# Hybrid boundary: resource lifecycle, the 80-pixel grayscale transform, clamping, packing, and loop control remain C.\n"
        "# The early_data $v1 binding emits no bytes; it preserves the two retail global loads and the pre-prologue schedule.\n"
        "# Hybrid scope: this zero-byte constraint preserves the active-index load before the stack-output reload.\n"
        "# It emits no instruction; KMC otherwise swaps the independent loads and advances the cursor too early.\n"
        : : "r" (active_index));
    *(u16 **)(data + (active_index * 4) + 0x70) = loaded_pixels;

    if ((arg1 & 0xFF) != 0) {
        pixels = loaded_pixels;
        for (index = 0; index < 80; index++, pixels++) {
            u16 pixel = *pixels;
            u32 packed = pixel & 0xFFFF;
            f32 intensity;
            u32 first_channel;
            u32 red;
            u32 green;
            u32 blue;

            intensity = ((f32)(s32)(packed >> 11) * 0.59f)
                + ((f32)(s32)((packed >> 6) & 0x1F) * 0.3f)
                + ((f32)(s32)((packed >> 1) & 0x1F) * 0.11f)
                + 0.5f;
            if (31.0f < intensity) {
                intensity = 31.0f;
            }
            first_channel = (u32)intensity;
            red = first_channel;
            green = (u32)intensity;
            blue = (u32)(intensity * 0.8f);
            *pixels = (red << 11) | (green << 6) | (blue << 1) | (pixel & 1);
        }
    }
}

