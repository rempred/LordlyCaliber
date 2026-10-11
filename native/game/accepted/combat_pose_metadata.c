typedef unsigned int u32;
typedef unsigned char u8;

extern int func_002015C8(int, int, int, int);
extern u32 *func_00206174(u32);

/* The leading byte indexes the record-length table in descriptor 10. */
#define RECORD_BYTES(token) (*(u8 *)(0x801CEEE0 + (token)))

/* These five public functions share one native compiler section. Its final
 * assembler alignment supplies the eight bytes in the last accepted owner.
 */
void *func_00204EE0(u32 handle, u32 index)
{
    u32 *directory = func_00206174(handle);
    void *result;

    if (directory == 0) {
        result = 0;
    } else {
        result = (void *)(directory[index] + (u32)directory);
    }
    return result;
}

void func_00204F24(void)
{
}

void func_00204F2C(void)
{
}

u32 func_00204F34(int a, int b, int c, int d, u32 directoryIndex,
                  u32 selected, u8 *out0, u8 *out1, u8 *out2)
{
    u32 *directory = func_00206174(func_002015C8(a, b, c, d));
    u8 *record;
    u32 count, index;

    if (directory != 0) {
        record = (u8 *)(directory[directoryIndex] + (u32)directory);
    } else {
        record = 0;
    }
    count = *record++;
    for (index = 0; index < count; index++) {
        if (index == selected) {
            switch (RECORD_BYTES(*record)) {
            case 2:
                if (out0) *out0 = record[1];
                break;
            case 3:
                if (out0) *out0 = record[1];
                if (out1) *out1 = record[2];
                break;
            case 4:
                if (out0) *out0 = record[1];
                if (out1) *out1 = record[2];
                if (out2) *out2 = record[3];
                break;
            }
            return *record;
        }
        record += RECORD_BYTES(*record);
    }
    return 0;
}

u32 func_002050AC(u32 handle, u32 directoryIndex, u32 selected,
                  u8 *out0, u8 *out1, u8 *out2)
{
    u32 *directory = func_00206174(handle);
    u8 *record;
    u32 count, index;

    if (directory != 0) {
        record = (u8 *)(directory[directoryIndex] + (u32)directory);
    } else {
        record = 0;
    }
    count = *record++;
    for (index = 0; index < count; index++) {
        if (index == selected) {
            switch (RECORD_BYTES(*record)) {
            case 2:
                if (out0) *out0 = record[1];
                break;
            case 3:
                if (out0) *out0 = record[1];
                if (out1) *out1 = record[2];
                break;
            case 4:
                if (out0) *out0 = record[1];
                if (out1) *out1 = record[2];
                if (out2) *out2 = record[3];
                break;
            }
            return *record;
        }
        record += RECORD_BYTES(*record);
    }
    return 0;
}
