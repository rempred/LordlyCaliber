#include "game/chapter_card.h"

extern void *func_0002dfb8(void *buffer, u32 key);
extern void *func_0002e138(u32 key);
extern s32 resource_free(u8 *buffer);
extern void *func_00070F30(s32 size);
extern void memcpy_aligned(void *destination, const void *source, s32 count);

void func_002878F4(void)
{
    u8 *buffer = func_0002dfb8(0, 0x01890C90);
    TitleImageHeader *image;
    s32 rowBytes;
    s32 payloadBytes;

    D_8023E588 = func_0002e138(((u32 *)buffer)[17]);
    D_8023E584 = func_0002e138(((u32 *)buffer)[18]);
    D_8023E580 = func_0002e138(((u32 *)buffer)[19]);
    resource_free(buffer);

    image = D_8023E584;
    rowBytes = func_0028ac88(image->field03, image->width);
    payloadBytes = rowBytes * ((TitleImageHeader *)D_8023E584)->height;
    /* Reuse the pointer local after releasing the directory to retain KMC's allocation. */
    buffer = func_00070F30(payloadBytes * 2);
    memcpy_aligned(buffer, (u8 *)D_8023E584 + 8, payloadBytes);
    memcpy_aligned(buffer + payloadBytes, (u8 *)D_8023E584 + 8, payloadBytes);

    resource_free(D_8023E584);
    D_8023E584 = buffer;
    D_8023E58C = 0;
    D_8023E590 = 160;
}
