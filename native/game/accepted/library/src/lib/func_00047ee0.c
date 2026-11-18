#include "game/story_event_entry.h"

typedef struct {
    u32 words[25];
} EventContext;

extern u8 D_80197B00;
extern u8 D_80197B01;
extern u8 D_80197B02;
extern u8 D_80197B03;
extern u16 D_80197B04;
extern s32 D_80197B14;

extern s32 resource_free(u8 *resource);
extern void *func_8009DBB8(void *destination, u32 key);
extern void *func_0002e138(u32 key);
extern s32 func_00046854(s32 entry);

s32 func_00047ee0(s32 row, s32 index)
{
    EventContext saved;
    volatile u8 *status;
    volatile u8 *selection;
    u32 *directory;
    u32 key;
    u8 *program;
    s32 result;

    saved = *(EventContext *)&D_80197B00;
    status = &D_80197B02;
    *status = 0;
    D_80197B03 = 2;
    D_80197B04 = 0;
    /* Keep the row-byte address separate until the context restore. */
    selection = status - 2;
    *selection = row;
    D_80197B01 = index;
    status = selection;

    directory = func_8009DBB8(0, 0x3A5668);
    key = directory[D_80197B00];
    resource_free((u8 *)directory);
    program = func_0002e138(key);
    func_00046854((s32)(program + ((u16 *)program)[D_80197B01]));
    resource_free(program);

    result = D_80197B14;
    *(EventContext *)status = saved;
    return result;
}
