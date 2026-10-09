#include "common/types.h"

typedef struct {
    u8 text[0x500];
    u8 gap500[0x30];
    u8 *pointers[48];
    u8 field5F0, field5F1, field5F2, field5F3;
    u8 field5F4, field5F5, field5F6, field5F7;
    u8 gap5F8[8];
    s32 field600;
} ShopBufferView;

typedef struct {
    u8 gap00[0xD0];
    ShopBufferView *fieldD0;
} ShopWindowBufferView;

extern u8 D_80219C20[];
extern void *func_80070F30(s32 size);
extern void func_80093380(void *buffer, s32 size);
extern void func_80093060(void *source, void *destination, s32 size);
extern void func_801805B4(void *buffer, s32 mode, s32 item);

void func_0019ADF0(ShopWindowBufferView *window, s32 item, u8 mode)
{
    u8 buffer[0x500];
    ShopBufferView *data;
    u8 *cursor;
    s32 count;
    s32 consumed;
    /* This call-crossing word lifetime lets KMC allocate the masked item
     * before scheduling the mask back beside its later test. */
    s32 item_half = item & 0xFFFF;

    window->fieldD0 = func_80070F30(0x604);
    func_80093380(buffer, 0x500);
    if (item_half != 0) {
        func_801805B4(buffer, mode & 0xFF, item_half);
    } else {
        func_80093060(D_80219C20, buffer, 0x500);
    }
    data = window->fieldD0;
    data->field600 = item;
    func_80093060(buffer, data, 0x500);
    cursor = data->text;
    count = 1;
    consumed = 0;
    data->pointers[0] = cursor;
    data->field5F7 = 0;
    /* Keep both real delimiter bodies explicit. KMC allocates their shared
     * index/address values before merging the identical machine-code tail. */
    do {
        u16 first = cursor[0];
        u8 second = cursor[1];
        if ((first < 0x81U) ||
            (((first >= 0xA0U) && (first < 0xE0U)) | (first >= 0xFDU))) {
            if ((first == 0x40) & (second == 0x6E)) {
                cursor[0] = 0;
                cursor += 2;
                data->pointers[count] = cursor;
                count++;
                consumed += 2;
                if (count >= 48) {
                    consumed = 0x500;
                }
            } else {
                cursor++;
                consumed++;
            }
        } else if ((first == 0x81) & (second == 0x97)) {
            cursor[0] = 0;
            cursor += 2;
            data->pointers[count] = cursor;
            count++;
            consumed += 2;
            if (count >= 48) {
                consumed = 0x500;
            }
        } else {
            cursor += 2;
            consumed += 2;
        }
    } while (consumed < 0x500);
    data->field5F0 = 0xFF;
    data->field5F4 = 2;
    data->field5F5 = count;
    data->field5F1 = 0;
    data->field5F2 = 0;
    data->field5F6 = 0xF;
}
