#include "common/types.h"

/* The loader copies 0x64 bytes before resetting the live context. */
typedef struct {
    u8 state00[8];
    s16 field08;
    u8 state0A[0x16];
    u32 data20[0x10];
    u16 field60;
    u16 field62;
} EventContext;

extern s16 D_80197B08;

extern u16 D_8018F1A0;
extern u8 D_801936A8;
extern u8 D_80197B00;
extern u8 D_80197B02;
extern s16 D_80197B06;
extern volatile s32 D_80197B18;
extern volatile s32 D_80197B1C;
extern u16 D_80197B60;

extern s32 resource_free(u8 *resource);
extern void func_80093380(void *destination, s32 bytes);
extern void func_80080998(void *destination, void *source, s32 bytes);
extern void *func_8009DBB8(void *destination, u32 key);
extern void *func_0002e138(u32 key);

void func_00048294(void)
{
    volatile s32 *primary = &D_80197B18;
    u8 *old_primary = (u8 *)*primary;
    EventContext saved;
    EventContext *context;
    s32 field08;
    /* Ordered directory reads keep each branch complete before its free. */
    volatile u32 *directory;
    u32 primary_key;
    u32 secondary_key;

    D_8018F1A0 = 0;
    resource_free(old_primary);
    resource_free((u8 *)D_80197B1C);
    field08 = D_80197B08;
    if ((field08 != 1) & (~field08 != 0)) {
        D_80197B08 = -1;
    }

    if (D_801936A8 != 0) {
        if (D_80197B02 == 0) {
            s16 saved_field08;
            u16 saved_field60;

            saved = *(EventContext *)((u8 *)primary - 0x18);
            context = (EventContext *)&D_80197B00;
            func_80093380(context, sizeof(*context));
            saved_field08 = saved.field08;
            saved_field60 = saved.field60;
            D_80197B08 = saved_field08;
            D_80197B60 = saved_field60;
            func_80080998(context->data20, saved.data20, sizeof(saved.data20));
            D_80197B06 = D_801936A8;
        }
        directory = func_8009DBB8(0, 0x3A5668);
        primary_key = directory[D_80197B06];
        secondary_key = directory[65];
        resource_free((u8 *)directory);
    } else {
        saved = *(EventContext *)((u8 *)primary - 0x18);
        context = (EventContext *)&D_80197B00;
        func_80093380(context, sizeof(*context));
        D_80197B08 = saved.field08;
        func_80080998(context->data20, saved.data20, sizeof(saved.data20));
        D_80197B06 = D_801936A8;
        directory = func_8009DBB8(0, 0x3A5668);
        primary_key = directory[D_80197B06];
        secondary_key = directory[70];
        resource_free((u8 *)directory);
    }

    /* Keep the first program store ahead of the next decoder call. */
    do {
        D_80197B18 = (s32)func_0002e138(primary_key);
    } while (0);
    D_80197B1C = (s32)func_0002e138(secondary_key);
}
