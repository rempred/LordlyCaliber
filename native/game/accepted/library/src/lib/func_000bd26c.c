typedef unsigned char u8;

typedef struct Entry {
    u8 bytes[14];
} Entry;

typedef struct Data {
    u8 unk_0000[0x10D4];
    Entry entries[1];
} Data;

extern int func_0016B088(u8, u8, u8, u8, int);
extern u8 *D_80196AF8;

int func_000bd26c(int arg0)
{
    u8 values[5];
    int index;
    u8 *output;
    u8 *entry;
    u8 *lookup;
    u8 item;
    int sentinel;

    index = 0;
    sentinel = 0xFF;
    output = values;
    arg0 &= 0xFFFF;
    entry = (*(Data **)0x80196AF8)->entries[arg0].bytes;
    do {
        item = (entry + index)[2];
        if (item != sentinel) {
            lookup = D_80196AF8;
            lookup += item * 54;
            *output = lookup[0x1199];
        } else {
            *output = 0;
        }
        index++;
        output++;
    } while (index < 5);

    return func_0016B088(values[0], values[1], values[2], values[3],
                         values[4]) & 0xFF;
}
