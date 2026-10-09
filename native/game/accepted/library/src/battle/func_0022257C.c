typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

typedef struct {
    u32 words[20];
} LocalCopy;

void func_001F0E64(LocalCopy *copy, u32 selector);
s32 func_001F197C(void *object, LocalCopy *copy, s32 *out);

s32 func_0022257C(void *record, s32 selector, s32 *out)
{
    LocalCopy copy;

    /* The five retail copy iterations each transfer four words: retain the
     * complete 0x50-byte envelope, including its final pointer word. */
    copy = *(LocalCopy *)record;
    func_001F0E64(&copy, selector);
    return func_001F197C(
        *(void **)(*(u8 **)((u8 *)record + 0x4C) + 0x40), &copy, out);
}
