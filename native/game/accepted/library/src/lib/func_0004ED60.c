typedef unsigned short u16;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct {
    s16 terminal_class;
    s16 environment;
    s16 field_4;
    s16 field_6;
} Func0004ED60Launch;

u32 func_8009DAF4(u32 resource);
void *func_80071C04(u32 size);
void func_8009DBB8(void *destination, u32 resource);
s32 func_800712C4(void *pointer);
u32 func_8007A7E0(void *source);
void func_8007A110(void *destination, void *source);

s32 func_0004ed60(s32 selector, Func0004ED60Launch *launch)
{
    register u32 *buffer asm("$17");
    u32 flags;
    u32 resource;
    u32 *decoded;
    s32 word_count;
    u32 unused[2];

    flags = 0;
    buffer = func_80071C04(func_8009DAF4(0x019A8804));
    func_8009DBB8(buffer, 0x019A8804);
    selector *= 4;
    selector += (u32)buffer;
    resource = *(u32 *)selector;
    func_800712C4(buffer);

    buffer = func_80071C04(func_8009DAF4(resource));
    func_8009DBB8(buffer, resource);
    selector = func_8007A7E0(buffer);
    decoded = func_80071C04(selector);
    func_8007A110(decoded, buffer);
    func_800712C4(buffer);

    word_count = selector / 4;
    selector = decoded[word_count - 1];
    if ((selector & 0xFF000000) == 0xFF000000) {
        selector &= 0xFF;
    } else {
        selector = 0;
    }
    launch->terminal_class = selector;

    selector = 0;
    if (word_count > 0) {
        do {
            if (decoded[selector] == 0x80000008) {
                flags |= 4;
                launch->field_6 = 1;
            }
            if (decoded[selector] == 0x80000007) {
                flags |= 2;
                launch->field_4 = decoded[selector + 1];
            }
            if (decoded[selector] == 0x80000006) {
                flags |= 1;
                launch->environment = decoded[selector + 1];
            }
            if ((flags == 4) || (decoded[selector] == 0x80000001)) {
                break;
            }
            selector++;
        } while (selector < word_count);
    }
    return func_800712C4(decoded);
}

asm(".word 0\n.word 0\n.size func_0004ed60, .-func_0004ed60");
