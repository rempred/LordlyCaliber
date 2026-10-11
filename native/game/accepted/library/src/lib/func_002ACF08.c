typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;

extern u8 D_801976E8;

extern void *func_0020C478(s32 rowIndex);
extern s32 func_0020BFF8(void *row);
extern void func_00207C08(s32 handle);
extern void func_001F0F90(void *object);
extern void func_001F1050(void *object);
extern void func_00023780(void *pointer, u32 size);
extern void func_002ACA3C(s32 unitSelector);

#define WORD(p, o) (*(s32 *)((u8 *)(p) + (o)))

/*
 * Director opcode 0x96 roster reset.
 *
 * Walks the twenty Actor-input rows. For each row that func_0020BFF8 selects,
 * releases the handle at +0x50, clears and releases the three linked object
 * pairs at +0x0/+0xC, +0x4/+0x10, and +0x8/+0x14, and zeroes the 0xF8-byte row.
 * Then appends deployed unit selector 30 and records it as the current second
 * roster unit (event property FD). Field meanings are structural observations,
 * not verified semantics.
 */
void func_002ACF08(void)
{
    s32 rowIndex;
    u32 linkIndex;
    void *row;
    void **links; /* links[i] is an object, links[i + 3] its partner (+0xC) */
    void *object;

    rowIndex = 0;
    do {
        row = func_0020C478(rowIndex);
        if (func_0020BFF8(row) != 0) {
            links = (void **)row;
            func_00207C08(WORD(row, 0x50));
            /* Indexing from one base keeps the partner access as an offset of
             * the same strength-reduced pointer; a second cursor would need
             * another callee-saved register. */
            linkIndex = 0;
            do {
                object = links[linkIndex];
                if (object != 0) {
                    WORD(object, 0x18) = 0;
                    WORD(links[linkIndex + 3], 0x18) = 0;
                    func_001F0F90(links[linkIndex]);
                    func_001F1050(links[linkIndex + 3]);
                    links[linkIndex] = 0;
                    links[linkIndex + 3] = 0;
                }
                linkIndex += 1;
            } while (linkIndex < 3);
            func_00023780(row, 0xF8);
        }
        rowIndex += 1;
    } while (rowIndex < 20);
    func_002ACA3C(30);
    D_801976E8 = 30;
}
