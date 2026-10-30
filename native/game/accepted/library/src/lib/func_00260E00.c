#include "common/types.h"

extern void *D_80220210[];
extern u16 D_80220D40;
extern void *D_80220D44;
extern s32 D_80220D48;
extern void **D_80220F20;
extern s32 func_8009DAF4(void *data);
extern void func_8009DBB8(void *dest, void *source);

void func_00260E00(u16 code, s32 index)
{
    void *data;
    s32 size;

    if (index > 0) {
        data = D_80220210[code];
        if (data != 0) {
            if ((D_80220D40 != code) || (D_80220D44 != data) ||
                (D_80220D48 != index)) {
                size = func_8009DAF4(data);
                if (index >= size / 4) {
                    /* Preserve the separate retail failure loops. */
                    for (;;) {
                    }
                }
                if ((size == 0) | (size > 0x7FFF)) {
                    for (;;) {
                    }
                }
                func_8009DBB8(D_80220F20, data);
                size = func_8009DAF4(D_80220F20[index]);
                if ((size == 0) | (size > 0x7FFF)) {
                    for (;;) {
                    }
                }
                func_8009DBB8(D_80220F20, D_80220F20[index]);
                return;
            }
            return;
        }
    }
    for (;;) {
    }
}
