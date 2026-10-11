#include "common/types.h"

extern void *D_80219F48[50];
extern void *D_8021A128, *D_8021A12C, *D_8021A130;
extern void resource_free(void *resource);

void func_0019BCA0(void)
{
    s32 index = 0;
    void **cursor = D_80219F48;

    do {
        void *resource = *cursor;
        if (resource != 0) {
            resource_free(resource);
        }
        index++;
        cursor++;
    } while (index < 50);

    resource_free(D_8021A128);
    resource_free(D_8021A12C);
    resource_free(D_8021A130);
}
