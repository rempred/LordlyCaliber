#include "interfaces.h"

/* Complete original body 5760..581C, including all ID paths and cleanup. */
s32 func_00005760(s32 id)
{
    void *record;
    s32 result = 0;

    if (id == 0x0E) {
        record = resource_alloc(0x10);
        result = func_00005A88(record) != 0;
    } else if (id == 0x0F) {
        record = resource_alloc(0x4AE8);
        if (func_00005978(record) != 0) {
            result = 1;
        }
    } else {
        void *scratch = resource_alloc(0x1850);
        if (func_0000581C(id, scratch) != 0) {
            result = 1;
        }
        record = scratch;
    }

    resource_free(record);
    if (result == 0) {
        func_0000539C(id);
    }
    return result;
}
