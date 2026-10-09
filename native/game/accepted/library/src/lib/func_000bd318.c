typedef unsigned char u8;

typedef struct Entry {
    u8 bytes[14];
} Entry;

typedef struct Data {
    u8 unk_0000[0x10D4];
    Entry entries[1];
} Data;

int func_000bd318(int arg0)
{
    int count;
    int index;
    u8 *entry;

    count = 11;
    index = 0;
    arg0 &= 0xFFFF;
    entry = (*(Data **)0x80196AF8)->entries[arg0].bytes;
    do {
        count -= (entry + index)[2] != 0xFF;
        index++;
    } while (index < 5);

    return count & 0xFF;
}
