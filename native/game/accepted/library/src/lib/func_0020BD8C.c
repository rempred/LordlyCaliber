#include "common/types.h"

typedef void (*Callback)(s16);

typedef struct {
    u8 unk0[0xC];
    Callback callback;
} CallbackHolder;

extern s8 D_801CFC70;

void func_0020BD8C(s32 value)
{
    Callback callback = (*(CallbackHolder **)0x801D0810)->callback;

    if (callback != 0) {
        callback((s16)value);
    } else {
        D_801CFC70 = 0;
    }
}
