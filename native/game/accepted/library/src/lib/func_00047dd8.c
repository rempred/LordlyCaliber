#include "common/types.h"

extern volatile s32 D_80197B0C;
extern u16 D_80197B04;
/* The world-map selector occupies the byte at 0x80196A9A. */
#define WORLD_MAP_SELECTOR (*(u8 *)0x80196A9A)
extern u8 D_801936A8;
extern volatile s32 D_80197B18;
extern volatile s32 D_80197B1C;

extern s32 func_00046854(s32 entry);
extern s32 resource_free(u8 *resource);
extern void func_00048294(void);
extern s32 func_0004813C(s32 request);

s32 func_00047dd8(s32 unused)
{
    s32 request;
    s32 result;
    u8 *program;
    u8 *entry;
    s32 index;

    request = D_80197B0C;
    D_80197B04 = 0;
    if (request == 1) {
        index = WORLD_MAP_SELECTOR & 0x7F;
        program = (u8 *)D_80197B18;
        goto select_entry;
    } else if (request == 0x40) {
        index = WORLD_MAP_SELECTOR & 0x7F;
        program = (u8 *)D_80197B1C;
select_entry:
        entry = program + ((u16 *)program)[index];
        goto invoke_entry;
    } else if (request == 0x80) {
        index = WORLD_MAP_SELECTOR & 0x7F;
        program = (u8 *)D_80197B1C;
        result = func_00046854((s32)(program + ((u16 *)program)[index]));
        D_801936A8 = 0;
        return result;
    } else {
        return 0;
    }

invoke_entry:
    result = func_00046854((s32)entry);
    if (result == 0) {
        return 0;
    }
    resource_free((u8 *)D_80197B18);
    resource_free((u8 *)D_80197B1C);
    D_80197B18 = 0;
    D_80197B1C = 0;
    func_00048294();
    func_0004813C(1);
    return result;
}
