typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

extern s32 D_8022A950;
extern s32 D_8022A954;
extern u32 *D_8022A958;
extern u32 *D_8022A95C;
extern u16 D_80196F60[];

u32 *func_8009DBB8(u32 *destination, u32 resource);
void func_800712C4(void *pointer);
u32 func_8009DAF4(u32 resource);
void *func_80070F30(u32 size);
u32 func_8007A7E0(void *source);
void func_8007A110(void *destination, void *source);

void func_00283FA8(s32 selector, s32 continuation)
{
    u32 *table;
    u32 resource;
    u32 *compressed;
    u32 decoded_size;
    u32 word_count;
    u32 index;
    u32 *word;
    u32 mask;
    u32 tag;
    u16 *translations;
    u32 unused[2];

    if (continuation == 0) {
        table = func_8009DBB8(0, 0x019A8804);
        if (D_8022A958 != 0) {
            func_800712C4(D_8022A958);
        }
        if (D_8022A95C != 0) {
            func_800712C4(D_8022A95C);
        }
        D_8022A95C = 0;
    } else {
        if ((D_8022A95C != 0) && (D_8022A958 != 0)) {
            func_800712C4(D_8022A958);
        } else {
            D_8022A954 = D_8022A950;
            D_8022A95C = D_8022A958;
        }
        table = func_8009DBB8(0, 0x0189099E);
    }

    resource = table[selector];
    compressed = func_80070F30(func_8009DAF4(resource));
    func_8009DBB8(compressed, resource);
    decoded_size = func_8007A7E0(compressed);
    D_8022A958 = func_80070F30(decoded_size);
    func_8007A110(D_8022A958, compressed);
    func_800712C4(compressed);
    func_800712C4(table);

    word_count = decoded_size >> 2;
    index = 0;
    if (word_count != 0) {
        mask = ~0xFF;
        tag = 0x08880000;
        translations = D_80196F60;
        word = D_8022A958;
        do {
            u32 value = *word;
            index++;
            if ((value & mask) == tag) {
                *word = translations[value & 0xFF];
            }
            word++;
        } while (index < word_count);
    }
    D_8022A950 = 0;
}
